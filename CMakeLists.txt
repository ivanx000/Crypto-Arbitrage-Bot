cmake_minimum_required(VERSION 3.20)

project(CryptoArbitrageBot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG v11.4.5
)

# Required for Binance/Coinbase wss:// endpoints.
set(USE_TLS ON CACHE BOOL "Enable TLS in ixwebsocket" FORCE)

FetchContent_MakeAvailable(nlohmann_json ixwebsocket)

add_executable(crypto_arbitrage_bot
  src/BinanceListener.cpp
  src/CoinbaseListener.cpp
  src/Executor.cpp
  src/ExchangeManager.cpp
  src/StrategyEngine.cpp
  src/main.cpp
)

target_include_directories(crypto_arbitrage_bot
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(crypto_arbitrage_bot
  PRIVATE
    nlohmann_json::nlohmann_json
    ixwebsocket
)
