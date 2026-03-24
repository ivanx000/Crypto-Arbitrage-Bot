#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>

#include "ExchangeManager.hpp"
#include "Executor.hpp"
#include "Types.hpp"

class StrategyEngine {
public:
    StrategyEngine(ExchangeManager& exchange_manager,
                   Executor& executor,
                   double fees_total,
                   double slippage_estimate);
    ~StrategyEngine();

    StrategyEngine(const StrategyEngine&) = delete;
    StrategyEngine& operator=(const StrategyEngine&) = delete;

    void Start();
    void Stop();

private:
    void OnExchangeManagerUpdate();
    void ProcessingLoop();
    void EvaluateSpread();

    ExchangeManager& exchange_manager_;
    double fees_total_;
    double slippage_estimate_;

    std::atomic<bool> running_{false};
    std::atomic<bool> pending_update_{false};

    std::thread worker_thread_;
    std::mutex signal_mutex_;
    std::condition_variable signal_cv_;

    mutable std::shared_mutex snapshot_mutex_;
    std::map<std::string, MarketData> latest_snapshot_;

    Executor& executor_;
    std::atomic<std::int64_t> last_update_timestamp_ns_{0};
};