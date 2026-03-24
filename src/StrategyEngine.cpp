#include "StrategyEngine.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>

StrategyEngine::StrategyEngine(ExchangeManager& exchange_manager,
                                                             Executor& executor,
                                                             double fees_total,
                                                             double slippage_estimate)
    : exchange_manager_(exchange_manager),
            executor_(executor),
      fees_total_(fees_total),
            slippage_estimate_(slippage_estimate) {
    exchange_manager_.SetUpdateCallback([this] { OnExchangeManagerUpdate(); });
}

StrategyEngine::~StrategyEngine() {
    exchange_manager_.SetUpdateCallback(nullptr);
    Stop();
}

void StrategyEngine::Start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    worker_thread_ = std::thread(&StrategyEngine::ProcessingLoop, this);
}

void StrategyEngine::Stop() {
    bool was_running = running_.exchange(false);
    if (!was_running) {
        return;
    }

    signal_cv_.notify_all();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void StrategyEngine::OnExchangeManagerUpdate() {
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    last_update_timestamp_ns_.store(now_ns);

    pending_update_.store(true);
    signal_cv_.notify_one();
}

void StrategyEngine::ProcessingLoop() {
    while (running_.load()) {
        {
            std::unique_lock<std::mutex> lock(signal_mutex_);
            signal_cv_.wait(lock, [this] {
                return !running_.load() || pending_update_.load();
            });
        }

        if (!running_.load()) {
            break;
        }

        pending_update_.store(false);

        const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto update_ns = last_update_timestamp_ns_.load();
        if (update_ns > 0 && now_ns >= update_ns) {
            const auto latency_ns = now_ns - update_ns;
            const auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::nanoseconds(latency_ns));
            executor_.UpdateInternalLatency(latency_ms);
        }

        {
            std::unique_lock<std::shared_mutex> lock(snapshot_mutex_);
            latest_snapshot_ = exchange_manager_.GetLatestTopOfBookSnapshot();
        }

        {
            std::shared_lock<std::shared_mutex> lock(snapshot_mutex_);
            executor_.UpdateMarketSnapshot(latest_snapshot_);
        }

        EvaluateSpread();
    }
}

void StrategyEngine::EvaluateSpread() {
    std::shared_lock<std::shared_mutex> lock(snapshot_mutex_);
    if (latest_snapshot_.size() < 2) {
        return;
    }

    const auto binance_it = latest_snapshot_.find("Binance");
    const auto coinbase_it = latest_snapshot_.find("Coinbase");
    if (binance_it == latest_snapshot_.end() || coinbase_it == latest_snapshot_.end()) {
        return;
    }

    const MarketData& binance = binance_it->second;
    const MarketData& coinbase = coinbase_it->second;

    const double spread_binance_to_coinbase =
        (binance.bid_price - coinbase.ask_price) - (fees_total_ + slippage_estimate_);
    const double spread_coinbase_to_binance =
        (coinbase.bid_price - binance.ask_price) - (fees_total_ + slippage_estimate_);

    auto log_alert = [&](const std::string& exchange_a,
                         const std::string& exchange_b,
                         double spread,
                         double quote_ask_price) {
        if (spread <= 0.0 || quote_ask_price <= 0.0) {
            return;
        }

        const double profit_pct = (spread / quote_ask_price) * 100.0;
        std::cout << "[HIGH-PRIORITY][ARBITRAGE] Spread>0 between "
                  << exchange_a << " and " << exchange_b
                  << " | spread=" << spread
                  << " | potential_profit=" << std::fixed << std::setprecision(4)
                  << profit_pct << "%"
                  << std::endl;
    };

    log_alert("Binance(bid)", "Coinbase(ask)", spread_binance_to_coinbase, coinbase.ask_price);
    log_alert("Coinbase(bid)", "Binance(ask)", spread_coinbase_to_binance, binance.ask_price);

    if (spread_binance_to_coinbase > 0.0) {
        executor_.executeTrade("Coinbase", "Binance", 1.0);
    }

    if (spread_coinbase_to_binance > 0.0) {
        executor_.executeTrade("Binance", "Coinbase", 1.0);
    }
}