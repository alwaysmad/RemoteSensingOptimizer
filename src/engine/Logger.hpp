// src/engine/Logger.hpp
#pragma once

#include <concepts>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace svk
{

class Logger
{
public:
	// Non-copyable, non-movable
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger(Logger&&) = delete;
	Logger& operator=(Logger&&) = delete;

	// =========================================================================
	//  ANSI Color Codes & Settings
	// =========================================================================
	static constexpr std::string_view COLOR_RED    = "\033[1;31m";
	static constexpr std::string_view COLOR_YELLOW = "\033[1;33m";
	static constexpr std::string_view COLOR_WHITE  = "\033[1;37m";
	static constexpr std::string_view COLOR_GRAY   = "\033[90m";
	static constexpr std::string_view COLOR_RESET  = "\033[0m";

	// =========================================================================
	//  Console logging: cLog (plain), cDebug, cInfo, cWarn, cError
	// =========================================================================
	template<typename... Args>
	inline void cLog(std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		writeRaw(std::cout, msg.view());
	}

	template<typename... Args>
	inline void cDebug(std::format_string<Args...> fmt, Args&&... args) const
		{ logConsole(std::cout, COLOR_GRAY, fmt, std::forward<Args>(args)...); }

	template<typename... Args>
	inline void cInfo(std::format_string<Args...> fmt, Args&&... args) const
		{ logConsole(std::cout, COLOR_WHITE, fmt, std::forward<Args>(args)...); }

	template<typename... Args>
	inline void cWarn(std::format_string<Args...> fmt, Args&&... args) const
		{ logConsole(std::cout, COLOR_YELLOW, fmt, std::forward<Args>(args)...); }

	template<typename... Args>
	inline void cError(std::format_string<Args...> fmt, Args&&... args) const
		{ logConsole(std::cerr, COLOR_RED, fmt, std::forward<Args>(args)...); }

	// =========================================================================
	//  File logging: fLog (plain), fDebug, fInfo, fWarn, fError
	// =========================================================================
	template<typename... Args>
	inline void fLog(std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		writeRawFile(msg.view());
	}

	template<typename... Args>
	inline void fDebug(std::format_string<Args...> fmt, Args&&... args) const
		{ logFile("[Debug] ", fmt, std::forward<Args>(args)...); }

	template<typename... Args>
	inline void fInfo(std::format_string<Args...> fmt, Args&&... args) const
		{ logFile("[Info] ", fmt, std::forward<Args>(args)...); }

	template<typename... Args>
	inline void fWarn(std::format_string<Args...> fmt, Args&&... args) const
		{ logFile("[Warn] ", fmt, std::forward<Args>(args)...); }

	template<typename... Args>
	inline void fError(std::format_string<Args...> fmt, Args&&... args) const
		{ logFile("[Error] ", fmt, std::forward<Args>(args)...); }

	// =========================================================================
	//  Contextual logging (Thread ID & Source Location) delegating to base methods
	// =========================================================================
	template<typename... Args>
	inline void cDebugCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cDebug("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void cInfoCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cInfo("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void cWarnCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cWarn("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void cErrorCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cError("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void fDebugCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		fDebug("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void fInfoCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		fInfo("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void fWarnCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		fWarn("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void fErrorCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		fError("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	// =========================================================================
	//  Constructors & Destructor
	// =========================================================================
	explicit Logger(bool enableColors = true, const std::filesystem::path& logPath = {} )
		: m_useColors(enableColors), m_logPath(logPath)
	{
		if (!m_logPath.empty())
		{
			m_file.open(m_logPath, std::ios::out | std::ios::trunc);
			if (!m_file.is_open())
				{ onFileOpenFail(); }
		}
	}

	inline ~Logger()
	{
		const std::lock_guard<std::mutex> lock(m_fileMutex);
		if (m_file.is_open())
			{ m_file.flush(); m_file.close(); }
	}

private:
	// Stack-first string formatting helper to eliminate duplicate formatting code
	template <size_t N = 2048>
	struct StackMsg
	{
		char buf[N];
		size_t size{0};
		std::string heap;

		template <typename... Args>
		inline StackMsg(std::format_string<Args...> fmt, Args&&... args)
		{
			const auto res = std::format_to_n(buf, N, fmt, std::forward<Args>(args)...);
			if (res.size <= N) [[likely]]
				{ size = res.size; }
			else [[unlikely]]
			{ 
				heap = std::format(fmt, std::forward<Args>(args)...);
				size = heap.size();
			}
		}

		[[nodiscard]] inline std::string_view view() const noexcept
		{
			if (heap.empty()) [[likely]]
				{ return std::string_view(buf, size); }
			else [[unlikely]]
				{ return std::string_view(heap); }
		}
	};

	template <typename... Args>
	inline void logConsole(std::ostream& os, std::string_view color, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		writeColored(os, color, msg.view());
	}

	template <typename... Args>
	inline void logFile(std::string_view prefix, std::format_string<Args...> fmt, Args&&... args) const
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		writeFile(prefix, msg.view());
	}

	static constexpr std::string_view extractFileName(std::string_view fullPath) noexcept
	{
		const auto lastSlash = fullPath.find_last_of("/\\");
		return (lastSlash == std::string_view::npos) ? fullPath : fullPath.substr(lastSlash + 1);
	}

	static inline size_t getThreadId() noexcept
	{
		thread_local const size_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
		return tid;
	}

	[[noreturn]] void onFileOpenFail() const
	{
		const std::string err = std::format(
			"svk::Logger: Failed to open log file at '{}' (errno: {})",
			m_logPath.string(), std::strerror(errno));
		std::cerr << COLOR_RED << "[FATAL] " << err << COLOR_RESET << '\n';
		throw std::runtime_error(err);
	}

	[[noreturn]] void onFileNotOpen() const
	{
		const std::string err = std::format(
			"svk::Logger: Attempted to write to '{}', but file is not open.",
			m_logPath.string());
		std::cerr << COLOR_RED << "[FATAL] " << err << COLOR_RESET << '\n';
		throw std::runtime_error(err);
	}

	[[noreturn]] void onFileWriteFail() const
	{
		const std::string err = std::format(
			"svk::Logger: Failed to write to log file '{}' (stream error state).",
			m_logPath.string());
		std::cerr << COLOR_RED << "[FATAL] " << err << COLOR_RESET << '\n';
		throw std::runtime_error(err);
	}

	inline void writeColored(std::ostream& os, std::string_view color, std::string_view msg) const
	{
		const std::lock_guard<std::mutex> lock(m_consoleMutex);
		if (m_useColors)
		{
			os.write(color.data(), static_cast<std::streamsize>(color.size()));
			os.write(msg.data(), static_cast<std::streamsize>(msg.size()));
			os.write(COLOR_RESET.data(), static_cast<std::streamsize>(COLOR_RESET.size()));
		}
		else
			{ os.write(msg.data(), static_cast<std::streamsize>(msg.size())); }
		os.put('\n');
	}

	inline void writeRaw(std::ostream& os, std::string_view msg) const
	{
		const std::lock_guard<std::mutex> lock(m_consoleMutex);
		os.write(msg.data(), static_cast<std::streamsize>(msg.size()));
		os.put('\n');
	}

	inline void writeFile(std::string_view prefix, std::string_view msg) const
	{
		const std::lock_guard<std::mutex> lock(m_fileMutex);
		if (!m_file.is_open()) [[unlikely]]
			{ onFileNotOpen(); }

		m_file.write(prefix.data(), static_cast<std::streamsize>(prefix.size()));
		m_file.write(msg.data(), static_cast<std::streamsize>(msg.size()));
		m_file.put('\n');
		m_file.flush();

		if (m_file.fail()) [[unlikely]]
			{ onFileWriteFail(); }
	}

	inline void writeRawFile(std::string_view msg) const
	{
		const std::lock_guard<std::mutex> lock(m_fileMutex);
		if (!m_file.is_open()) [[unlikely]]
			{ onFileNotOpen(); }

		m_file.write(msg.data(), static_cast<std::streamsize>(msg.size()));
		m_file.put('\n');
		m_file.flush();

		if (m_file.fail()) [[unlikely]]
			{ onFileWriteFail(); }
	}

	const std::filesystem::path m_logPath;
	mutable std::ofstream m_file;
	mutable std::mutex m_consoleMutex;
	mutable std::mutex m_fileMutex;
	const bool m_useColors;
};

} // namespace svk