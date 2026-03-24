#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "Types.hpp"

class BinanceListener {
public:
    using MarketDataCallback = std::function<void(const MarketData&)>;

    BinanceListener(std::string stream, MarketDataCallback callback = nullptr);
    ~BinanceListener();

    BinanceListener(const BinanceListener&) = delete;
    BinanceListener& operator=(const BinanceListener&) = delete;

    void Start();
    void Stop();

    void SetMarketDataCallback(MarketDataCallback callback);

private:
    void Run();
    void HandleMessage(const std::string& payload);
    static std::string ToLower(std::string value);

    std::string stream_;
    MarketDataCallback on_market_data_;
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::mutex callback_mutex_;
};