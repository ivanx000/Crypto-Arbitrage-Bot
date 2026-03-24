#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>

#include "BinanceListener.hpp"
#include "CoinbaseListener.hpp"
#include "Types.hpp"

class ExchangeManager {
public:
    using UpdateCallback = std::function<void()>;

    ExchangeManager(std::string binance_stream, std::string coinbase_product_id);
    ~ExchangeManager();

    ExchangeManager(const ExchangeManager&) = delete;
    ExchangeManager& operator=(const ExchangeManager&) = delete;

    void Start();
    void Stop();

    std::map<std::string, MarketData> GetLatestTopOfBookSnapshot() const;
    void SetUpdateCallback(UpdateCallback callback);

private:
    void OnMarketData(const MarketData& data);

    BinanceListener binance_listener_;
    CoinbaseListener coinbase_listener_;

    mutable std::shared_mutex latest_data_mutex_;
    std::map<std::string, MarketData> latest_top_of_book_;

    mutable std::mutex callback_mutex_;
    UpdateCallback on_update_;
};