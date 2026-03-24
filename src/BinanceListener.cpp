#include "BinanceListener.hpp"

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

BinanceListener::BinanceListener(std::string stream, MarketDataCallback callback)
    : stream_(ToLower(std::move(stream))), on_market_data_(std::move(callback)) {
    if (stream_.find("@") == std::string::npos) {
        stream_ += "@bookTicker";
    }
}

BinanceListener::~BinanceListener() {
    Stop();
}

void BinanceListener::Start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    worker_thread_ = std::thread(&BinanceListener::Run, this);
}

void BinanceListener::Stop() {
    bool was_running = running_.exchange(false);
    if (!was_running) {
        return;
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void BinanceListener::SetMarketDataCallback(MarketDataCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    on_market_data_ = std::move(callback);
}

void BinanceListener::Run() {
    const std::string url = "wss://stream.binance.com:9443/ws/" + stream_;
    constexpr std::chrono::seconds reconnect_delay(3);
    constexpr std::chrono::milliseconds poll_interval(200);

    while (running_.load()) {
        std::mutex disconnect_mutex;
        std::condition_variable disconnect_cv;
        bool disconnected = false;

        ix::WebSocket web_socket;
        web_socket.setUrl(url);
        web_socket.setOnMessageCallback(
            [this, &disconnected, &disconnect_mutex, &disconnect_cv](const ix::WebSocketMessagePtr& message) {
                if (message->type == ix::WebSocketMessageType::Message) {
                    HandleMessage(message->str);
                    return;
                }

                if (message->type == ix::WebSocketMessageType::Close) {
                    std::cerr << "Binance socket closed. Code=" << message->closeInfo.code
                              << " Reason=" << message->closeInfo.reason << std::endl;
                    {
                        std::lock_guard<std::mutex> lock(disconnect_mutex);
                        disconnected = true;
                    }
                    disconnect_cv.notify_one();
                    return;
                }

                if (message->type == ix::WebSocketMessageType::Error) {
                    std::cerr << "Binance socket error: " << message->errorInfo.reason << std::endl;
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

void BinanceListener::HandleMessage(const std::string& payload) {
    try {
        const nlohmann::json json_payload = nlohmann::json::parse(payload);

        MarketData data;
        data.symbol = json_payload.at("s").get<std::string>();
        data.bid_price = ParseJsonNumber(json_payload.at("b"));
        data.ask_price = ParseJsonNumber(json_payload.at("a"));
        data.bid_size = ParseJsonNumber(json_payload.at("B"));
        data.ask_size = ParseJsonNumber(json_payload.at("A"));
        data.exchange_name = "Binance";

        MarketDataCallback callback;
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            callback = on_market_data_;
        }

        if (callback) {
            callback(data);
        }
    } catch (const std::exception& error) {
        std::cerr << "Failed to parse Binance payload: " << error.what() << std::endl;
    }
}

std::string BinanceListener::ToLower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}