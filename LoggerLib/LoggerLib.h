#pragma once
#include <iostream>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>

using namespace std;

// allows for passing multiple modes to the logger
enum class LogLevel : uint8_t {
    TRACE = 1,
    DEBUG = 2,
    INFO = 4,
    WARNING = 8,
    ERROR = 16
};

struct Message {
    LogLevel level;
    string message;
    bool reset;
};

class Logger
{
    friend struct std::default_delete<Logger>;
public:
    static Logger* GetInstance(const string& log_path = ".", const string& log_name = "app.log", uint8_t log_levels = 31);

    // Delete copy constructor and assignment operator to prevent copying of the singleton instance
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Public logging method
    void Log(LogLevel level, const string& message, bool reset = false);

private:
    // Constructor/Destructor
    Logger(const string& log_path, const string& log_name, uint8_t log_levels);
    ~Logger();

    void ProcessQueue(); // Background worker loop to process log messages
    void WriteToFile(const Message& msg); // Helper method to write a format and write a log message to the disk
    string GetLevelName(LogLevel level); // Helper method to get the string representation of a log level
    void RotateLogFile(const string& full_path);

    static Logger* _instance;
    string _log_path;
    string _log_name;
	uint8_t _log_levels;
	ofstream _log_file;

    // Asynchronous Queue Components
	queue<Message> _message_queue;
	mutex _queue_mutex;
	condition_variable _queue_cv;
	thread _worker_thread;
	atomic<bool> _stop;
};