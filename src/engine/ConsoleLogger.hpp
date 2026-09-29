// src/engine/ConsoleLogger.hpp
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>
#include <mutex>
#include <source_location>
#include <string_view>
#include <thread>
#include <utility>

#ifndef SVK_CONSOLE_LOGGER_BUFFER_SIZE
#define SVK_CONSOLE_LOGGER_BUFFER_SIZE 2048
#endif

namespace svk
{

// =============================================================================
//  ConsoleLogger: High-performance, zero-heap, noexcept console logger
// =============================================================================
class ConsoleLogger
{
public:
	// Non-copyable, non-movable
	ConsoleLogger(const ConsoleLogger&) = delete;
	ConsoleLogger& operator=(const ConsoleLogger&) = delete;
	ConsoleLogger(ConsoleLogger&&) = delete;
	ConsoleLogger& operator=(ConsoleLogger&&) = delete;

	// ANSI Color Codes
	static constexpr std::string_view COLOR_RED    = "\033[1;31m";
	static constexpr std::string_view COLOR_YELLOW = "\033[1;33m";
	static constexpr std::string_view COLOR_WHITE  = "\033[1;37m";
	static constexpr std::string_view COLOR_GRAY   = "\033[90m";
	static constexpr std::string_view COLOR_RESET  = "\033[0m";

	explicit inline ConsoleLogger(bool enableColors = true) noexcept : m_useColors(enableColors) {}

	// =========================================================================
	//  Console logging methods (strictly noexcept, zero heap allocations)
	// =========================================================================
	template<typename... Args>
	inline void cLog(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		writeRaw(stdout, msg.view());
	}

	template<typename... Args>
	inline void cDebug(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		logColored(stdout, COLOR_GRAY, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args>
	inline void cInfo(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		logColored(stdout, COLOR_WHITE, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args>
	inline void cWarn(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		logColored(stdout, COLOR_YELLOW, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args>
	inline void cError(std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		logColored(stderr, COLOR_RED, fmt, std::forward<Args>(args)...);
	}

	// =========================================================================
	//  Contextual logging (Thread ID & Source Location) delegating to base methods
	// =========================================================================
	template<typename... Args>
	inline void cDebugCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cDebug("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void cInfoCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cInfo("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void cWarnCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cWarn("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

	template<typename... Args>
	inline void cErrorCtx(std::source_location loc, std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		cError("[T:{:x}] [{}:{}] {}", getThreadId(), extractFileName(loc.file_name()), loc.line(), msg.view());
	}

private:
	// Stack-only buffer with safe truncation: strictly zero heap allocations, strictly noexcept
	struct StackMsg
	{
		char buf[SVK_CONSOLE_LOGGER_BUFFER_SIZE];
		size_t size{0};

		template <typename... Args>
		inline StackMsg(std::format_string<Args...> fmt, Args&&... args) noexcept
		{
			const auto res = std::format_to_n(buf, sizeof(buf) - 1, fmt, std::forward<Args>(args)...);
			if (res.size < sizeof(buf) - 1) [[likely]]
			{
				size = res.size;
			}
			else [[unlikely]]
			{
				static constexpr std::string_view marker = "... [TRUNCATED]";
				size = sizeof(buf) - 1;
				if (size >= marker.size())
				{
					std::memcpy(buf + (size - marker.size()), marker.data(), marker.size());
				}
			}
		}

		[[nodiscard]] inline std::string_view view() const noexcept
		{
			return std::string_view(buf, size);
		}
	};

	template <typename... Args>
	inline void logColored(std::FILE* stream, std::string_view color, std::format_string<Args...> fmt, Args&&... args) const noexcept
	{
		const StackMsg msg(fmt, std::forward<Args>(args)...);
		writeColored(stream, color, msg.view());
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

	inline void writeColored(std::FILE* stream, std::string_view color, std::string_view msg) const noexcept
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		if (m_useColors) [[likely]]
		{
			std::fwrite(color.data(), 1, color.size(), stream);
			std::fwrite(msg.data(), 1, msg.size(), stream);
			std::fwrite(COLOR_RESET.data(), 1, COLOR_RESET.size(), stream);
		}
		else
		{
			std::fwrite(msg.data(), 1, msg.size(), stream);
		}
		std::fputc('\n', stream);
	}

	inline void writeRaw(std::FILE* stream, std::string_view msg) const noexcept
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		std::fwrite(msg.data(), 1, msg.size(), stream);
		std::fputc('\n', stream);
	}

	mutable std::mutex m_mutex;
	const bool m_useColors;
};

} // namespace svk
