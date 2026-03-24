# Crypto Arbitrage Bot (C++20)

A modular crypto arbitrage bot prototype built in C++20 with CMake.

This project streams top-of-book market data from multiple exchanges, standardizes it into a common market data model, computes cross-exchange spreads, performs risk checks, and writes mock trade tickets to disk.

## Features

- C++20 + CMake build system
- Real-time WebSocket listeners using IXWebSocket
- JSON parsing using nlohmann/json
- Standardized `MarketData` model across exchanges
- Multi-exchange aggregation with thread-safe snapshots
- Strategy engine with spread calculations and alerting
- Mock execution layer with risk checks and trade logging
- CLI dashboard (500ms refresh) with clean Ctrl+C shutdown

## Current Exchange Feeds

- Binance (`btcusdt@bookTicker`)
- Coinbase (`BTC-USD` ticker channel)

## Project Structure

```text
.
├── CMakeLists.txt
├── include/
│   ├── BinanceListener.hpp
│   ├── CoinbaseListener.hpp
│   ├── ExchangeManager.hpp
│   ├── Executor.hpp
│   ├── StrategyEngine.hpp
│   └── Types.hpp
├── src/
│   ├── BinanceListener.cpp
│   ├── CoinbaseListener.cpp
│   ├── ExchangeManager.cpp
│   ├── Executor.cpp
│   ├── StrategyEngine.cpp
│   └── main.cpp
└── build/                # local build artifacts (gitignored)
```

## Requirements

- macOS (or another platform with a compatible C++20 compiler)
- CMake >= 3.20
- Ninja (recommended)
- Xcode Command Line Tools (Apple Clang)
- Homebrew (macOS dependency management)

## Install Dependencies (macOS)

```bash
xcode-select --install
brew install cmake ninja pkg-config openssl@3
```

If Homebrew binaries are not on your PATH:

```bash
echo 'export PATH="/opt/homebrew/bin:$PATH"' >> ~/.zprofile
source ~/.zprofile
```

## Build

From repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix openssl@3)"
cmake --build build -j
```

## Run

```bash
./build/crypto_arbitrage_bot
```

Press `Ctrl+C` to shut down cleanly.

## Runtime Behavior

- Listener threads collect exchange market data
- `ExchangeManager` stores latest top-of-book per exchange
- `StrategyEngine` is triggered on updates and evaluates spread:

$$
Spread = (Price_{ExchangeA\_Bid} - Price_{ExchangeB\_Ask}) - (Fees_{Total} + Slippage_{Estimate})
$$

- If spread is positive, a high-priority alert is printed
- `Executor` performs mock execution if risk checks pass

## Risk Checks (MVP)

A mock trade is blocked when:

- Spread is below `0.1%`
- Internal latency is above `100ms`

## Logging

- Mock trade tickets are appended to `trades.log`
- Includes timestamp, buy/sell exchanges, prices, spread %, and theoretical profit

## Security and Secrets

- Do not commit API keys, certificates, or secret files
- `.gitignore` already excludes common secret formats and `.env` files

## Troubleshooting

- `cmake: command not found`
  - Ensure Homebrew is installed and `/opt/homebrew/bin` is on PATH
- `TLS support is not enabled on this platform`
  - Ensure IXWebSocket is built with TLS enabled (already configured in `CMakeLists.txt`)
- No live data appears
  - Verify internet access and exchange endpoint availability

## Disclaimer

This software is for educational and research purposes only.
It does not execute real trades by default and should not be used in production without extensive testing, risk controls, and compliance review.
