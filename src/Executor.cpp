#include "Executor.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

Executor::Executor(std::string log_file_path)
    : log_file_path_(std::move(log_file_path)) {}

void Executor::UpdateMarketSnapshot(const std::map<std::string, MarketData>& snapshot) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    latest_market_snapshot_ = snapshot;
}

void Executor::UpdateInternalLatency(std::chrono::milliseconds latency) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    internal_latency_ = latency;
}

bool Executor::executeTrade(std::string buyExch, std::string sellExch, double amount) {
    if (amount <= 0.0) {
        return false;
    }

    double buy_price = 0.0;
    double sell_price = 0.0;
    double spread_pct = 0.0;

    {
        std::shared_lock<std::shared_mutex> lock(state_mutex_);

        const auto buy_it = latest_market_snapshot_.find(buyExch);
        const auto sell_it = latest_market_snapshot_.find(sellExch);
        if (buy_it == latest_market_snapshot_.end() || sell_it == latest_market_snapshot_.end()) {
            return false;
        }

        buy_price = buy_it->second.ask_price;
        sell_price = sell_it->second.bid_price;
        if (buy_price <= 0.0) {
            return false;
        }

        spread_pct = ((sell_price - buy_price) / buy_price) * 100.0;
    }

    if (!PassRiskCheck(spread_pct)) {
        return false;
    }

    const double theoretical_profit = (sell_price - buy_price) * amount;
    AppendTicket(buyExch, sellExch, amount, buy_price, sell_price, spread_pct, theoretical_profit);
    return true;
}

bool Executor::PassRiskCheck(double spread_pct) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);

    if (spread_pct < 0.1) {
        std::cout << "[RISK-CHECK] Blocked trade: spread below 0.1% threshold ("
                  << spread_pct << "%)." << std::endl;
        return false;
    }

    if (internal_latency_.count() > 100) {
        std::cout << "[RISK-CHECK] Blocked trade: internal latency "
                  << internal_latency_.count() << "ms exceeds 100ms limit."
                  << std::endl;
        return false;
    }

    return true;
}

void Executor::AppendTicket(const std::string& buy_exch,
                            const std::string& sell_exch,
                            double amount,
                            double buy_price,
                            double sell_price,
                            double spread_pct,
                            double theoretical_profit) const {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &now_time);
#else
    localtime_r(&now_time, &local_time);
#endif

    std::ofstream out(log_file_path_, std::ios::app);
    if (!out.is_open()) {
        std::cerr << "Failed to open trade log file: " << log_file_path_ << std::endl;
        return;
    }

    out << "========== MOCK TRADE TICKET ==========" << '\n';
    out << "timestamp: " << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << '\n';
    out << "buy_exchange: " << buy_exch << '\n';
    out << "sell_exchange: " << sell_exch << '\n';
    out << "amount: " << std::fixed << std::setprecision(6) << amount << '\n';
    out << "buy_price: " << std::fixed << std::setprecision(6) << buy_price << '\n';
    out << "sell_price: " << std::fixed << std::setprecision(6) << sell_price << '\n';
    out << "spread_pct: " << std::fixed << std::setprecision(4) << spread_pct << "%" << '\n';
    out << "theoretical_profit: " << std::fixed << std::setprecision(8) << theoretical_profit << '\n';
    out << "=======================================" << '\n';
}