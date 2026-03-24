#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "Types.hpp"

class CoinbaseListener {
public:
    using MarketDataCallback = std::function<void(const MarketData&)>;

    CoinbaseListener(std::string product_id, MarketDataCallback callback = nullptr);
    ~CoinbaseListener();

    CoinbaseListener(const CoinbaseListener&) = delete;
    CoinbaseListener& operator=(const CoinbaseListener&) = delete;

    void Start();
    void Stop();

    void SetMarketDataCallback(MarketDataCallback callback);

private:
    void Run();
    void HandleMessage(const std::string& payload);
    static std::string ToUpper(std::string value);

    std::string product_id_;
    MarketDataCallback on_market_data_;
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::mutex callback_mutex_;
};