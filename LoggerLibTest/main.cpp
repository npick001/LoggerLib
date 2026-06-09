#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include "../LoggerLib/LoggerLib.h" 

// Configuration for the benchmarks
constexpr int FIREHOSE_MESSAGES = 1'000'000;
constexpr int CONTENTION_THREADS = 16;
constexpr int CONTENTION_MESSAGES_PER_THREAD = 100'000;

void BenchmarkSingleThreadLatency(Logger* logger) {
    std::cout << "--- Starting Single-Threaded Firehose Benchmark ---" << std::endl;
    std::cout << "Writing " << FIREHOSE_MESSAGES << " messages on a single thread..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < FIREHOSE_MESSAGES; ++i) {
        logger->Log(LogLevel::INFO, "Firehose benchmark message payload.");
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::nano> total_ns = end_time - start_time;
    std::chrono::duration<double, std::milli> total_ms = end_time - start_time;

    double average_latency = total_ns.count() / FIREHOSE_MESSAGES;

    std::cout << "Total Time: " << total_ms.count() << " ms" << std::endl;
    std::cout << "Average Producer Latency: " << std::fixed << std::setprecision(2) << average_latency << " ns/message" << std::endl;
    std::cout << "Goal: < 500 nanoseconds per call" << std::endl << std::endl;
}

void BenchmarkMultiThreadContention(Logger* logger) {
    std::cout << "--- Starting Multi-Threaded Contention Benchmark ---" << std::endl;
    std::cout << "Spawning " << CONTENTION_THREADS << " threads, each writing " << CONTENTION_MESSAGES_PER_THREAD << " messages..." << std::endl;

    std::vector<std::thread> workers;

    // Use an atomic to synchronize the start time of all threads for maximum contention
    std::atomic<bool> start_flag{ false };

    auto worker_task = [&logger, &start_flag]() {
        // Spin-lock until the main thread says go
        while (!start_flag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        for (int i = 0; i < CONTENTION_MESSAGES_PER_THREAD; ++i) {
            logger->Log(LogLevel::INFO, "Contention benchmark message payload from thread.");
        }
        };

    // Spin up the threads
    for (int i = 0; i < CONTENTION_THREADS; ++i) {
        workers.emplace_back(worker_task);
    }

    // Start the timer and release the threads simultaneously
    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);

    // Wait for all threads to finish pushing to the queue
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    int total_messages = CONTENTION_THREADS * CONTENTION_MESSAGES_PER_THREAD;
    std::chrono::duration<double, std::milli> total_ms = end_time - start_time;
    double throughput = (total_messages / total_ms.count()) * 1000.0;

    std::cout << "Total Messages Queued: " << total_messages << std::endl;
    std::cout << "Total Time: " << total_ms.count() << " ms" << std::endl;
    std::cout << "Producer Throughput: " << std::fixed << std::setprecision(0) << throughput << " messages/second" << std::endl;
    std::cout << "Goal: 1,000,000+ messages per second" << std::endl << std::endl;
}

int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "    Asynchronous Logger Benchmark        " << std::endl;
    std::cout << "=========================================" << std::endl;

    // 1. Initialize the Logger
    Logger* logger = Logger::GetInstance("logs", "benchmark.log", 31);

    // 2. Run the Single-Threaded Test
    BenchmarkSingleThreadLatency(logger);

    // 3. Run the Multi-Threaded Test
    BenchmarkMultiThreadContention(logger);

    std::cout << "Benchmarks complete. Waiting for the background worker to flush to disk..." << std::endl;

    // Note: The total throughput to disk depends on the worker thread completing. 
    // Since we use a Meyers' Singleton, the logger destructor will be called at program exit,
    // joining the worker thread and ensuring all messages are flushed.

    return 0;
}