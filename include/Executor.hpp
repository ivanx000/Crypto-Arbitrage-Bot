#pragma once

#include <chrono>
#include <map>
#include <shared_mutex>
#include <string>

#include "Types.hpp"

class Executor {
public:
    explicit Executor(std::string log_file_path = "trades.log");

    void UpdateMarketSnapshot(const std::map<std::string, MarketData>& snapshot);
    void UpdateInternalLatency(std::chrono::milliseconds latency);

    bool executeTrade(std::string buyExch, std::string sellExch, double amount);

private:
    bool PassRiskCheck(double spread_pct) const;
    void AppendTicket(const std::string& buy_exch,
                      const std::string& sell_exch,
                      double amount,
                      double buy_price,
                      double sell_price,
                      double spread_pct,
                      double theoretical_profit) const;

    std::string log_file_path_;

    mutable std::shared_mutex state_mutex_;
    std::map<std::string, MarketData> latest_market_snapshot_;
    std::chrono::milliseconds internal_latency_{0};
};