#include "Logger.hpp"

#include "EngineLog.hpp"
#include "LogSystem.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <utility>

namespace tcpengine
{
bool Logger::start(const LogSettings& settings)
{
	try {
		logsystem::start(settings);
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to start logging: " << e.what() << '\n';
		return false;
	}
}

bool Logger::isEnabled(LogSource source, LogLevel level)
{
	return logsystem::isEnabled(source, level);
}

void Logger::write(LogSource source, LogLevel level, std::string message)
{
	logsystem::write(source, level, message);
}

bool Logger::flush(std::optional<std::chrono::milliseconds> timeout)
{
	return logsystem::flush(timeout);
}

void Logger::installTerminateHandler()
{
	std::set_terminate([] {
		// 메모리 부족이나 sink 오류로 여기서 다시 던질 수 있음. 밖으로 나가면 abort()에 닿지 못하므로 모두 삼킴
		try {
			std::string message = "std::terminate() called";
			if (const std::exception_ptr exception = std::current_exception()) {
				try {
					std::rethrow_exception(exception);
				}
				catch (const std::exception& e) {
					message = std::string("Uncaught exception: ") + e.what();
				}
				catch (...) {
					message = "Uncaught unknown exception";
				}
			}
			write(LogSource::Engine, LogLevel::Error, std::move(message));
			// 로그 스레드 자신이 죽는 중이면 flush가 끝나지 않으므로 시간을 제한함
			flush(std::chrono::seconds(2));
		}
		catch (...) {
		}
		std::abort();
	});
}

ScopedLogFlush::~ScopedLogFlush()
{
	Logger::flush();
}

bool isLogEnabled(LogLevel level)
{
	return Logger::isEnabled(LogSource::App, level);
}

void writeLog(LogLevel level, std::string message)
{
	Logger::write(LogSource::App, level, std::move(message));
}

bool isEngineLogEnabled(LogLevel level)
{
	return Logger::isEnabled(LogSource::Engine, level);
}

void writeEngineLog(LogLevel level, std::string message)
{
	Logger::write(LogSource::Engine, level, std::move(message));
}
}
