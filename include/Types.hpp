#pragma once

#include <string>

struct MarketData {
    std::string symbol;
    double bid_price;
    double ask_price;
    double bid_size;
    double ask_size;
    std::string exchange_name;
};
