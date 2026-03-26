// src/engine/Logger.hpp
#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <format>
#include <utility>

namespace svk
{

class Logger
{
	// Owned by Application - console (Herald) and file (Scribe) logging
public:
	// No copy
	Logger(const Logger&) = delete;
	Logger& operator= (const Logger&) = delete;
	// Move is allowed
	Logger(Logger&&) noexcept = default;
	Logger& operator= (Logger&&) noexcept = default;

	// =========================================================================
	//  Console logging: cDebug, cInfo, cWarn, cError
	// =========================================================================
	template<typename... Args> inline void cDebug(std::format_string<Args...> fmt, Args&&... args) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		std::cout << COLOR_GRAY << std::format(fmt, std::forward<Args>(args)...) << COLOR_RESET << "\n";
	}
	template<typename... Args> inline void cInfo(std::format_string<Args...> fmt, Args&&... args) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		std::cout << COLOR_WHITE << std::format(fmt, std::forward<Args>(args)...) << COLOR_RESET << "\n";
	}
	template<typename... Args> inline void cWarn(std::format_string<Args...> fmt, Args&&... args) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		std::cout << COLOR_YELLOW << std::format(fmt, std::forward<Args>(args)...) << COLOR_RESET << "\n";
	}
	template<typename... Args> inline void cError(std::format_string<Args...> fmt, Args&&... args) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		std::cerr << COLOR_RED << std::format(fmt, std::forward<Args>(args)...) << COLOR_RESET << "\n";
	}

	// =========================================================================
	//  File logging: fDebug, fInfo, fWarn, fError
	// =========================================================================
	template<typename... Args> inline void fDebug(std::format_string<Args...> fmt, Args&&... args) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_file << "[Debug] " << std::format(fmt, std::forward<Args>(args)...) << "\n";
	}
	template<typename... Args> inline void fInfo(std::format_string<Args...> fmt, Args&&... args) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_file << "[Info] " << std::format(fmt, std::forward<Args>(args)...) << "\n";
	}
	template<typename... Args> inline void fWarn(std::format_string<Args...> fmt, Args&&... args) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_file << "[Warn] " << std::format(fmt, std::forward<Args>(args)...) << "\n";
	}
	template<typename... Args> inline void fError(std::format_string<Args...> fmt, Args&&... args) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_file << "[Error] " << std::format(fmt, std::forward<Args>(args)...) << "\n";
	}

	// ANSI Color Codes
	static constexpr std::string_view COLOR_RED    = "\033[1;31m";
	static constexpr std::string_view COLOR_YELLOW = "\033[1;33m";
	static constexpr std::string_view COLOR_WHITE  = "\033[1;37m";
	static constexpr std::string_view COLOR_GRAY   = "\033[90m";
	static constexpr std::string_view COLOR_RESET  = "\033[0m";

	explicit Logger(const std::string& logPath) { m_file.open(logPath); }
	~Logger() = default;

private:
	mutable std::ofstream m_file;
	mutable std::mutex m_mutex;
};

} // namespace svk
