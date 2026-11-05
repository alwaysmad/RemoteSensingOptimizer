// src/engine/ConsoleLogger.hpp
#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <format>
#include <mutex>
#include <source_location>
#include <string_view>
#include <thread>
#include <utility>

#ifndef SVK_CONSOLE_LOGGER_BUFFER_SIZE // can be overridden by the build system or user code
#define SVK_CONSOLE_LOGGER_BUFFER_SIZE 2048
#endif

// Forward declaration inside the namespace
namespace svk
{
class ConsoleLogger;
}

// =============================================================================
//  ConsoleLogger: High-performance, zero-heap, noexcept console logger
// =============================================================================
class svk::ConsoleLogger
{
private:
	mutable std::mutex m_mutex;
	const bool m_useColors;

	// No-heap output to any stream (stdotut or stderr) WITHOUT COLORS
	void writeRaw(std::FILE* stream, std::string_view msg) const noexcept
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		std::fwrite(msg.data(), 1, msg.size(), stream);
		std::fputc('\n', stream);
	}
	// No-heap output to any stream (stdotut or stderr) WITH COLORS
	void writeColored(std::FILE* stream, std::string_view color, std::string_view msg) const noexcept
	{
		if (m_useColors)
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			std::fwrite(color.data(), 1, color.size(), stream);
			std::fwrite(msg.data(), 1, msg.size(), stream);
			std::fwrite(COLOR_RESET.data(), 1, COLOR_RESET.size(), stream);
			std::fputc('\n', stream);
		}
		else
		{
			writeRaw(stream, msg);
		}
	}

	// Stack-only buffer with safe truncation: zero heap allocations,  noexcept
	struct StackMsg
	{
		static constexpr std::string_view marker = "...[TRUNCATED]";

		std::array<char, SVK_CONSOLE_LOGGER_BUFFER_SIZE> buf{};
		std::size_t size{0};

		template<typename... Args> explicit StackMsg(std::format_string<Args...> fmt, Args&&... args) noexcept
		{
			constexpr std::size_t max_content_size =
			    (SVK_CONSOLE_LOGGER_BUFFER_SIZE > 0) ? SVK_CONSOLE_LOGGER_BUFFER_SIZE - 1 : 0;

			const auto res = std::format_to_n(buf.data(), max_content_size, fmt, std::forward<Args>(args)...);
			const auto total_needed = static_cast<std::size_t>(res.size);

			if (total_needed <= max_content_size) [[likely]] { size = total_needed; }
			else [[unlikely]]
			{
				size = max_content_size;
				if constexpr (SVK_CONSOLE_LOGGER_BUFFER_SIZE >= marker.size() + 1)
				{
					std::ranges::copy(marker, buf.begin() + (size - marker.size()));
				}
			}
			buf[size] = '\0';
		}

		[[nodiscard]] constexpr std::string_view view() const noexcept { return std::string_view{buf.data(), size}; }

		[[nodiscard]] constexpr const char* c_str() const noexcept { return buf.data(); }
	};

	// base formatting output method
	template<typename... Args>
	void logColored(std::FILE* stream,
	                std::string_view color,
	                std::format_string<Args...> fmt,
	                Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		writeColored(stream, color, msg.view());
	}

	static constexpr std::string_view extractFileName(std::string_view fullPath) noexcept
	{
		const auto lastSlash = fullPath.find_last_of("/\\");
		return (lastSlash == std::string_view::npos) ? fullPath : fullPath.substr(lastSlash + 1);
	}

	static size_t getThreadId() noexcept
	{
		thread_local const size_t tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
		return tid;
	}

public:
	// Non-copyable, non-movable
	ConsoleLogger(const ConsoleLogger&) = delete;
	ConsoleLogger& operator=(const ConsoleLogger&) = delete;
	ConsoleLogger(ConsoleLogger&&) = delete;
	ConsoleLogger& operator=(ConsoleLogger&&) = delete;

	explicit ConsoleLogger(bool enableColors = true) noexcept : m_useColors(enableColors) {}
	~ConsoleLogger() noexcept = default;

	// ANSI Color Codes
	static constexpr std::string_view COLOR_RED = "\033[1;31m";
	static constexpr std::string_view COLOR_YELLOW = "\033[1;33m";
	static constexpr std::string_view COLOR_WHITE = "\033[1;37m";
	static constexpr std::string_view COLOR_GRAY = "\033[90m";
	static constexpr std::string_view COLOR_RESET = "\033[0m";

	// =========================================================================
	//  Console logging methods (strictly noexcept, zero heap allocations)
	// =========================================================================
	// raw (formatting) method with no colors
	template<typename... Args> void cLog(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		writeRaw(stdout, msg.view());
	}

	template<typename... Args> void cDebug(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		logColored(stdout, COLOR_GRAY, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args> void cInfo(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		logColored(stdout, COLOR_WHITE, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args> void cWarn(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		logColored(stdout, COLOR_YELLOW, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args> void cError(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		logColored(stderr, COLOR_RED, fmt, std::forward<Args>(args)...);
	}

	// =========================================================================
	//  Contextual logging (prints Thread ID and Source Location) delegating to base methods
	// =========================================================================
	template<typename... Args>
	void cDebugCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cDebug("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	void cInfoCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cInfo("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	void cWarnCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cWarn("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	void cErrorCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cError("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}
};