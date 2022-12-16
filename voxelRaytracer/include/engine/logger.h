#pragma once


enum class LogLevel
{
	info,
	warning,
	error
};

#define LOG_INFO(aMessage, ...) Logger::log(LogLevel::info, aMessage, __VA_ARGS__)
#define LOG_WARNING(aMessage, ...) Logger::log(LogLevel::warning, aMessage, __VA_ARGS__)
#define LOG_ERROR(aMessage, ...) Logger::log(LogLevel::error, aMessage, __VA_ARGS__)

class Logger
{
public:
	static void log(LogLevel aLevel, const char* aMessage, ...);
};