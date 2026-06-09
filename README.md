# LoggerLib

A lightweight, asynchronous C++ logging library designed for high throughput and low latency in both single-threaded and multi-threaded environments.

## Overview

`LoggerLib` provides an asynchronous queue-based logger with a dedicated background thread that flushes messages to disk. It supports multiple log levels and minimizes producer-side blocking by swapping message queues before writing batches to the log file.

## Features

- Asynchronous log enqueueing with a background writer thread
- Multiple logging levels: `TRACE`, `DEBUG`, `INFO`, `WARNING`, `ERROR`
- Singleton instance access via `Logger::GetInstance(...)`
- Double-buffered queue swap to reduce contention
- Batch writes with smart flush behavior for error/reset events
- Simple disk-backed `ofstream` logging

## Performance

This library is optimized for low-latency logging under heavy load:

- Single-threaded producer latency: approximately **140 nanoseconds per message** when writing **1,000,000 messages**
- Multi-threaded throughput: capable of **1,000,000 messages per second** on **16 producer threads**

These benchmarks show that `LoggerLib` can handle high-firehose logging workloads while keeping producer-side overhead very small.

## Project Structure

- `LoggerLib/` - static library source and headers
- `LoggerLibTest/` - benchmark/test executable
- `LoggerLib.slnx` - Visual Studio solution for the library and test project

## Build & Run

1. Open `LoggerLib.slnx` in Visual Studio.
2. Build the `LoggerLib` and `LoggerLibTest` projects.
3. Run `LoggerLibTest` to execute the benchmark driver.

The benchmark executable writes log output to the `logs` directory as configured in `LoggerLibTest/main.cpp`.

## Usage

Use the singleton logger instance in your application:

```cpp
Logger* logger = Logger::GetInstance("logs", "app.log", static_cast<uint8_t>(LogLevel::INFO) | static_cast<uint8_t>(LogLevel::ERROR));
logger->Log(LogLevel::INFO, "Application started.");
```

## Notes

- The library currently uses a singleton logger instance for simplicity.
- The log file path and active log levels are configurable through `GetInstance`.

## License

Refer to the repository or project owner for license details.
