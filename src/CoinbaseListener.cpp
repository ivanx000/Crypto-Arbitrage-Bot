#include "CoinbaseListener.hpp"

#include <chrono>
#include <condition_variable>
#include <cctype>
#include <exception>
#include <iostream>
#include <mutex>
#include <utility>

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

namespace {
double ParseJsonNumber(const nlohmann::json& value) {
    if (value.is_number()) {
        return value.get<double>();
    }

    if (value.is_string()) {
        return std::stod(value.get<std::string>());
    }

    throw std::runtime_error("Unsupported number format in payload");
}
}  // namespace

CoinbaseListener::CoinbaseListener(std::string product_id, MarketDataCallback callback)
    : product_id_(ToUpper(std::move(product_id))), on_market_data_(std::move(callback)) {}

CoinbaseListener::~CoinbaseListener() {
    Stop();
}

void CoinbaseListener::Start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    worker_thread_ = std::thread(&CoinbaseListener::Run, this);
}

void CoinbaseListener::Stop() {
    bool was_running = running_.exchange(false);
    if (!was_running) {
        return;
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void CoinbaseListener::SetMarketDataCallback(MarketDataCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_market_data_ = std::move(callback);
}

void CoinbaseListener::Run() {
    constexpr const char* url = "wss://ws-feed.exchange.coinbase.com";
    constexpr std::chrono::seconds reconnect_delay(3);
    constexpr std::chrono::milliseconds poll_interval(200);

    while (running_.load()) {
        std::mutex disconnect_mutex;
        std::condition_variable disconnect_cv;
        bool disconnected = false;

        ix::WebSocket web_socket;
        web_socket.setUrl(url);
        web_socket.setOnMessageCallback(
            [this, &web_socket, &disconnected, &disconnect_mutex, &disconnect_cv](
                const ix::WebSocketMessagePtr& message) {
                if (message->type == ix::WebSocketMessageType::Open) {
                    nlohmann::json subscribe_payload = {
                        {"type", "subscribe"},
                        {"channels", {{{"name", "ticker"}, {"product_ids", {product_id_}}}}}
                    };
                    web_socket.send(subscribe_payload.dump());
                    return;
                }

                if (message->type == ix::WebSocketMessageType::Message) {
                    HandleMessage(message->str);
                    return;
                }

                if (message->type == ix::WebSocketMessageType::Close) {
                    std::cerr << "Coinbase socket closed. Code=" << message->closeInfo.code
                              << " Reason=" << message->closeInfo.reason << std::endl;
                    {
                        std::lock_guard<std::mutex> lock(disconnect_mutex);
                        disconnected = true;
                    }
                    disconnect_cv.notify_one();
                    return;
                }

                if (message->type == ix::WebSocketMessageType::Error) {
                    std::cerr << "Coinbase socket error: " << message->errorInfo.reason << std::endl;
                    {
                        std::lock_guard<std::mutex> lock(disconnect_mutex);
                        disconnected = true;
                    }
                    disconnect_cv.notify_one();
                }
            });

        web_socket.start();

        {
            std::unique_lock<std::mutex> lock(disconnect_mutex);
            while (running_.load() && !disconnected) {
                disconnect_cv.wait_for(lock, poll_interval, [this, &disconnected] {
                    return !running_.load() || disconnected;
                });
            }
        }

        web_socket.stop();

        if (!running_.load()) {
            break;
        }

        std::this_thread::sleep_for(reconnect_delay);
    }
}

void CoinbaseListener::HandleMessage(const std::string& payload) {
    try {
        const nlohmann::json json_payload = nlohmann::json::parse(payload);
        if (!json_payload.contains("type")) {
            return;
        }

        const std::string event_type = json_payload.at("type").get<std::string>();
        if (event_type != "ticker") {
            return;
        }

        MarketData data;
        data.symbol = json_payload.value("product_id", product_id_);
        data.bid_price = ParseJsonNumber(json_payload.at("best_bid"));
        data.ask_price = ParseJsonNumber(json_payload.at("best_ask"));

        if (json_payload.contains("best_bid_size")) {
            data.bid_size = ParseJsonNumber(json_payload.at("best_bid_size"));
        } else if (json_payload.contains("bid_size")) {
            data.bid_size = ParseJsonNumber(json_payload.at("bid_size"));
        } else {
            data.bid_size = 0.0;
        }

        if (json_payload.contains("best_ask_size")) {
            data.ask_size = ParseJsonNumber(json_payload.at("best_ask_size"));
        } else if (json_payload.contains("ask_size")) {
            data.ask_size = ParseJsonNumber(json_payload.at("ask_size"));
        } else {
            data.ask_size = 0.0;
        }

        data.exchange_name = "Coinbase";

        MarketDataCallback callback;
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            callback = on_market_data_;
        }

        if (callback) {
            callback(data);
        }
    } catch (const std::exception& error) {
        std::cerr << "Failed to parse Coinbase payload: " << error.what() << std::endl;
    }
}

std::string CoinbaseListener::ToUpper(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return value;
}