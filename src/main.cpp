#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <string>

#include "ExchangeManager.hpp"
#include "Executor.hpp"
#include "StrategyEngine.hpp"
#include "Types.hpp"

namespace {
std::atomic<bool> g_shutdown_requested{false};

void HandleSignal(int signal) {
    if (signal == SIGINT) {
        g_shutdown_requested.store(true);
    }
}

double ComputeSpread(double bid_price, double ask_price, double fees_total, double slippage_estimate) {
    return (bid_price - ask_price) - (fees_total + slippage_estimate);
}
}  // namespace

int main() {
    std::signal(SIGINT, HandleSignal);

    constexpr double fees_total = 15.0;
    constexpr double slippage_estimate = 2.0;

    ExchangeManager exchange_manager("btcusdt@bookTicker", "BTC-USD");
    Executor executor("trades.log");
    StrategyEngine strategy_engine(exchange_manager, executor, fees_total, slippage_estimate);

    exchange_manager.Start();
    strategy_engine.Start();

    std::mutex dashboard_mutex;
    std::condition_variable dashboard_cv;

    double best_spread_so_far = std::numeric_limits<double>::lowest();
    std::string best_spread_path = "N/A";

    while (!g_shutdown_requested.load()) {
        {
            std::unique_lock<std::mutex> lock(dashboard_mutex);
            dashboard_cv.wait_for(lock, std::chrono::milliseconds(500), [] {
                return g_shutdown_requested.load();
            });
        }

        const std::map<std::string, MarketData> snapshot = exchange_manager.GetLatestTopOfBookSnapshot();
        const auto binance_it = snapshot.find("Binance");
        const auto coinbase_it = snapshot.find("Coinbase");

        double spread_binance_to_coinbase = 0.0;
        double spread_coinbase_to_binance = 0.0;

        if (binance_it != snapshot.end() && coinbase_it != snapshot.end()) {
            const MarketData& binance = binance_it->second;
            const MarketData& coinbase = coinbase_it->second;

            spread_binance_to_coinbase = ComputeSpread(
                binance.bid_price,
                coinbase.ask_price,
                fees_total,
                slippage_estimate);

            spread_coinbase_to_binance = ComputeSpread(
                coinbase.bid_price,
                binance.ask_price,
                fees_total,
                slippage_estimate);

            if (spread_binance_to_coinbase > best_spread_so_far) {
                best_spread_so_far = spread_binance_to_coinbase;
                best_spread_path = "Binance bid -> Coinbase ask";
            }

            if (spread_coinbase_to_binance > best_spread_so_far) {
                best_spread_so_far = spread_coinbase_to_binance;
                best_spread_path = "Coinbase bid -> Binance ask";
            }
        }

        std::cout << "\x1B[2J\x1B[H";
        std::cout << "===== Crypto Arbitrage Bot Dashboard =====" << std::endl;
        std::cout << "Press Ctrl+C to exit cleanly" << std::endl;
        std::cout << std::endl;

        if (binance_it != snapshot.end()) {
            const MarketData& binance = binance_it->second;
            std::cout << "Binance  | Bid: " << std::fixed << std::setprecision(2) << binance.bid_price
                      << " | Ask: " << binance.ask_price << std::endl;
        } else {
            std::cout << "Binance  | waiting for data..." << std::endl;
        }

        if (coinbase_it != snapshot.end()) {
            const MarketData& coinbase = coinbase_it->second;
            std::cout << "Coinbase | Bid: " << std::fixed << std::setprecision(2) << coinbase.bid_price
                      << " | Ask: " << coinbase.ask_price << std::endl;
        } else {
            std::cout << "Coinbase | waiting for data..." << std::endl;
        }

        std::cout << std::endl;
        std::cout << "Current spread (Binance->Coinbase): "
                  << std::fixed << std::setprecision(4) << spread_binance_to_coinbase << std::endl;
        std::cout << "Current spread (Coinbase->Binance): "
                  << std::fixed << std::setprecision(4) << spread_coinbase_to_binance << std::endl;

        if (best_spread_so_far != std::numeric_limits<double>::lowest()) {
            std::cout << "Best spread so far: " << std::fixed << std::setprecision(4)
                      << best_spread_so_far << " (" << best_spread_path << ")" << std::endl;
        } else {
            std::cout << "Best spread so far: waiting for both exchanges..." << std::endl;
        }
    }

    std::cout << "\nShutdown signal received. Stopping components..." << std::endl;
    strategy_engine.Stop();
    exchange_manager.Stop();
    std::cout << "Shutdown complete." << std::endl;

    return 0;
}
