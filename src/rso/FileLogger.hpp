// src/rso/FileLogger.hpp
#pragma once

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace svk
{

// =============================================================================
//  FileLogger: Reliable file logger for experiment results & valuable data
// =============================================================================
class FileLogger
{
public:
	// Non-copyable, non-movable
	FileLogger(const FileLogger&) = delete;
	FileLogger& operator=(const FileLogger&) = delete;
	FileLogger(FileLogger&&) = delete;
	FileLogger& operator=(FileLogger&&) = delete;

	explicit inline FileLogger(const std::filesystem::path& filePath) : m_filePath(filePath)
	{
		m_file.open(m_filePath, std::ios::out | std::ios::trunc);
		if (!m_file.is_open()) [[unlikely]]
		{
			const std::string err = std::format(
			    "svk::FileLogger: Failed to open file at '{}' (errno: {})", m_filePath.string(), std::strerror(errno));
			std::fprintf(stderr, "\033[1;31m[FATAL] %s\033[0m\n", err.c_str());
			throw std::runtime_error(err);
		}
	}

	inline ~FileLogger()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		if (m_file.is_open())
		{
			m_file.flush();
			m_file.close();
		}
	}

	[[nodiscard]] inline const std::filesystem::path& path() const noexcept { return m_filePath; }

	// Single logging method with heap allocation for comprehensive/large data output
	template<typename... Args> inline void log(std::format_string<Args...> fmt, Args&&... args) const
	{
		const std::string msg = std::format(fmt, std::forward<Args>(args)...);

		const std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_file.is_open()) [[unlikely]]
		{
			const std::string err =
			    std::format("svk::FileLogger: Attempted to write to '{}', but file is not open.", m_filePath.string());
			std::fprintf(stderr, "\033[1;31m[FATAL] %s\033[0m\n", err.c_str());
			throw std::runtime_error(err);
		}

		m_file.write(msg.data(), static_cast<std::streamsize>(msg.size()));
		m_file.put('\n');
		m_file.flush();

		if (m_file.fail()) [[unlikely]]
		{
			const std::string err =
			    std::format("svk::FileLogger: Failed to write to file '{}' (stream error state).", m_filePath.string());
			std::fprintf(stderr, "\033[1;31m[FATAL] %s\033[0m\n", err.c_str());
			throw std::runtime_error(err);
		}
	}

private:
	const std::filesystem::path m_filePath;
	mutable std::ofstream m_file;
	mutable std::mutex m_mutex;
};

} // namespace svk
