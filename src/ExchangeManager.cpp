#include "ExchangeManager.hpp"

#include <shared_mutex>
#include <utility>

ExchangeManager::ExchangeManager(std::string binance_stream, std::string coinbase_product_id)
    : binance_listener_(std::move(binance_stream), [this](const MarketData& data) { OnMarketData(data); }),
      coinbase_listener_(std::move(coinbase_product_id), [this](const MarketData& data) { OnMarketData(data); }) {}

ExchangeManager::~ExchangeManager() {
    Stop();
}

void ExchangeManager::Start() {
    binance_listener_.Start();
    coinbase_listener_.Start();
}

void ExchangeManager::Stop() {
    binance_listener_.Stop();
    coinbase_listener_.Stop();
}

std::map<std::string, MarketData> ExchangeManager::GetLatestTopOfBookSnapshot() const {
    std::shared_lock<std::shared_mutex> lock(latest_data_mutex_);
    return latest_top_of_book_;
}

void ExchangeManager::SetUpdateCallback(UpdateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_update_ = std::move(callback);
}

void ExchangeManager::OnMarketData(const MarketData& data) {
    {
        std::unique_lock<std::shared_mutex> lock(latest_data_mutex_);
        latest_top_of_book_[data.exchange_name] = data;
    }

    UpdateCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = on_update_;
    }

    if (callback) {
        callback();
    }
}