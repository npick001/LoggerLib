// LoggerLib.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "framework.h"
#include "LoggerLib.h"
#include <filesystem>
#include <sstream>
#include <iomanip>

Logger* Logger::GetInstance(const string& log_path, const string& log_name, uint8_t log_levels)
{
	// The Multiton Registry: Maps the file path to a specific Logger instance
	static unordered_map<string, unique_ptr<Logger>> instances;
	static mutex registry_mutex;

	// Use the combined path and name as the unique key
	string key = log_path + "/" + log_name;

	lock_guard<mutex> lock(registry_mutex);

	// If a logger for this specific file doesn't exist yet, create it
	if (instances.find(key) == instances.end()) {
		// We use 'new' instead of make_unique because the constructor is private
		instances[key] = unique_ptr<Logger>(new Logger(log_path, log_name, log_levels));
	}

	// Return the existing logger for this file
	return instances[key].get();
}

void Logger::Log(LogLevel level, const string& message, bool reset)
{
	// Bitmask check (make sure logging is enabled for the given level)
	if (!(static_cast<uint8_t>(level) & _log_levels)) {
		return; // skip logging if the level is not enabled
	}

	// Push message to queue
	Message msg{ level, message, reset };
	bool should_notify = false;
	{
		lock_guard<mutex> lock(_queue_mutex); // lock the queue for this scope
		should_notify = _message_queue.empty();
		_message_queue.push(move(msg));

		// mutex lock is automatically released here when lock goes out of scope
	} 

	if (should_notify) {
		_queue_cv.notify_one(); // let worker thread know there's a new message to process
	} 
}

Logger::Logger(const string& log_path, const string& log_name, uint8_t log_levels)
{
	_log_path = log_path;
	_log_name = log_name;
	_log_levels = log_levels;
	_stop = false;

	string full_path = _log_path + "/" + _log_name;

	// Back up the existing file to prevent data loss before opening a new one
	RotateLogFile(full_path);

	_log_file.open(full_path, ios_base::app);
	if (!_log_file.is_open()) {
		cerr << "CRITICAL: Logger failed to open log file at " << full_path << endl;
	}

	_message_queue = queue<Message>();
	_worker_thread = thread(&Logger::ProcessQueue, this);
}

Logger::~Logger()
{
	{
		lock_guard<mutex> lock(_queue_mutex);
		_stop = true;
	}
	_queue_cv.notify_one(); // wake up the background thread

	// clean up the background thread
	if (_worker_thread.joinable()) {
		_worker_thread.join();
	}

	if (_log_file.is_open()) {
		_log_file.flush();
		_log_file.close();
	}
}

// Background worker loop to process log messages (Double-buffered queue approach)
void Logger::ProcessQueue()
{
	while (true) {
		queue<Message> local_queue;

		{
			unique_lock<mutex> lock(_queue_mutex);
			_queue_cv.wait(lock, [this] { return !_message_queue.empty() || _stop; });

			if (_stop && _message_queue.empty()) {
				return;
			}

			// DOUBLE BUFFERING: Instantly swap the queues to minimize lock contention
			local_queue.swap(_message_queue);
		}

		// Process the entire batch without blocking the producer threads
		bool requires_flush = false;

		while (!local_queue.empty()) {
			const Message& msg = local_queue.front();

			WriteToFile(msg);

			// Smart flushing: Only flag for immediate flush on ERROR or explicit resets
			if (msg.level == LogLevel::ERROR || msg.reset) {
				requires_flush = true;
			}

			local_queue.pop();
		}

		// Flush the entire batch at once
		if (requires_flush && _log_file.is_open()) {
			_log_file.flush();
		}
	}
}

void Logger::WriteToFile(const Message& msg)
{
	if (!_log_file.is_open()) {
		return;
	}

	if (msg.reset) {
		_log_file.close();
		string full_path = _log_path + "/" + _log_name;
		_log_file.open(full_path, ios_base::trunc);
	}

	// Capture time stamp for the log message
	auto now = chrono::system_clock::now();
	time_t now_time_t = chrono::system_clock::to_time_t(now);
	char time_string[26];
	ctime_s(time_string, sizeof(time_string), &now_time_t);
	string timestamp(time_string);
	timestamp.pop_back(); // remove the newline character added by ctime_s

	_log_file << "[" << timestamp << "] [" << GetLevelName(msg.level) << "] - " << msg.message << "\n";
}

string Logger::GetLevelName(LogLevel level)
{
	switch (level) {
	case LogLevel::TRACE:   return "TRACE";
	case LogLevel::DEBUG:   return "DEBUG";
	case LogLevel::INFO:    return "INFO";
	case LogLevel::WARNING: return "WARNING";
	case LogLevel::ERROR:   return "ERROR";
	default:                return "UNKNOWN";
	}
}

void Logger::RotateLogFile(const string& full_path)
{
	if (filesystem::exists(full_path)) {
		// Generate a timestamp suitable for a filename (avoiding colons)
		auto now = chrono::system_clock::now();
		time_t now_time_t = chrono::system_clock::to_time_t(now);
		tm local_tm;
#ifdef _MSC_VER
		localtime_s(&local_tm, &now_time_t);
#else
		localtime_r(&now_time_t, &local_tm);
#endif

		stringstream ss;
		ss << put_time(&local_tm, "%Y-%m-%d_%H-%M-%S");

		// Create the "old_logs" directory if it doesn't exist
		filesystem::path backup_dir = filesystem::path(_log_path) / "backups";
		error_code ec;
		filesystem::create_directories(backup_dir, ec);

		if (ec && ec.value() != 0) {
			cerr << "WARNING: Failed to create old_logs directory at " << backup_dir << ": " << ec.message() << endl;
			backup_dir = _log_path; // Fallback to base path if folder creation fails
		}

		// Construct the new path inside the old_logs folder
		filesystem::path backup_path = backup_dir / (_log_name + "." + ss.str());

		filesystem::rename(full_path, backup_path, ec);
		if (ec) {
			cerr << "WARNING: Failed to rotate log file " << full_path << " to " << backup_path << ": " << ec.message() << endl;
		}
	}
}
