#pragma once

#include <cpr/cpr.h>

#include <chrono>
#include <ctime>
#include <exception>
#include <expected>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "quill.hpp"
#include "tgbotxx/tgbotxx.hpp"

namespace cielparser {

template <size_t N>
void TryNTimes(auto&& f) {
  for (size_t i = 0; i < N; ++i) {
    try {
      f();
      return;
    } catch (const std::exception& e) {
      if (i + 1 < N) {
        LOG_ERROR("exception caught, sleep for 5 second and retry: {}", e.what());
        std::this_thread::sleep_for(std::chrono::seconds(5));
      } else {
        LOG_ERROR("exception caught after {} retries: {}", N, e.what());
      }
    }
  }
}

inline std::vector<std::string> GetMatchedUrlsFromPattern(const std::string_view message, const std::regex& pattern) {
  using Iterator = std::regex_token_iterator<std::string_view::const_iterator>;
  return {Iterator{message.begin(), message.end(), pattern}, Iterator{}};
}

inline std::optional<cpr::Response> HttpGet(const std::string_view url, const cpr::Header& headers = {},
                                            const cpr::Parameters& params = {}) {
  static constexpr int kMaxRetries = 3;
  for (int attempt = 1; attempt <= kMaxRetries; ++attempt) {
    cpr::Response r = cpr::Get(cpr::Url{url}, params, headers);
    if (r.status_code == 200 && r.error.code == cpr::ErrorCode::OK) {
      return r;
    }
    LOG_ERROR("Download {} failed (attempt {}/{}), status_code = {}, error.code = {}, error.message = {}", url, attempt,
              kMaxRetries, r.status_code, std::to_string(r.error.code), r.error.message);
  }
  return std::nullopt;
}

inline std::filesystem::path SaveContents(const std::filesystem::path& download_dir, const std::string_view ext,
                                          const std::string_view download_link, const std::string_view file_contents) {
  while (true) {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    const auto* lt = std::localtime(&tt);
    const auto nanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() % 1'000'000'000;

    char time_buf[16];
    std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", lt);
    auto filepath = download_dir / std::format("{}_{:09d}{}", time_buf, nanos, ext);
    std::ofstream ofs(filepath, std::ios::out | std::ios::binary | std::ios::noreplace);
    if (!ofs) {
      continue;
    }
    ofs << file_contents;
    LOG_INFO("Downloaded {} in {}", download_link, filepath.string());
    return filepath;
  }
}

inline tgbotxx::Ptr<tgbotxx::ReplyParameters> MakeReplyParameters(const std::int32_t message_id) {
  const auto reply_params = std::make_shared<tgbotxx::ReplyParameters>();
  reply_params->messageId = message_id;
  return reply_params;
}

struct VideoInfo {
  unsigned int width{};
  unsigned int height{};
  unsigned int duration{};
};

inline uint32_t ReadBE32(const uint8_t* data) {
  return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

inline uint64_t ReadBE64(const uint8_t* data) { return (uint64_t(ReadBE32(data)) << 32) | ReadBE32(data + 4); }

inline VideoInfo GetVideoInfo(const std::filesystem::path& file_path) {
  VideoInfo info;
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file) {
    return info;
  }

  const auto file_size = static_cast<size_t>(file.tellg());
  if (file_size < 8) {
    return info;
  }

  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(file_size);
  file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));

  constexpr uint32_t kMvhd = 0x6D766864;
  constexpr uint32_t kTkhd = 0x746B6864;

  for (size_t pos = 0; pos + 8 < file_size; ++pos) {
    const uint32_t tag = ReadBE32(buffer.data() + pos);

    if (tag == kMvhd) {
      const uint8_t version = buffer[pos + 4];
      if (version == 0 && pos + 24 <= file_size) {
        const uint32_t timescale = ReadBE32(buffer.data() + pos + 16);
        const uint32_t duration = ReadBE32(buffer.data() + pos + 20);
        if (timescale > 0) {
          info.duration = static_cast<unsigned int>(duration / timescale);
        }
      } else if (version == 1 && pos + 36 <= file_size) {
        const uint32_t timescale = ReadBE32(buffer.data() + pos + 24);
        const uint64_t duration = ReadBE64(buffer.data() + pos + 28);
        if (timescale > 0) {
          info.duration = static_cast<unsigned int>(duration / timescale);
        }
      }
    } else if (tag == kTkhd && info.width == 0) {
      const uint8_t version = buffer[pos + 4];
      if (version == 0 && pos + 88 <= file_size) {
        info.width = ReadBE32(buffer.data() + pos + 80) >> 16;
        info.height = ReadBE32(buffer.data() + pos + 84) >> 16;
      } else if (version == 1 && pos + 100 <= file_size) {
        info.width = ReadBE32(buffer.data() + pos + 92) >> 16;
        info.height = ReadBE32(buffer.data() + pos + 96) >> 16;
      }
    }
  }

  return info;
}

enum struct ErrorCode {
  HttpError,
  ParseError,
  ServiceUnavailable,
  AccessDenied,
  ScriptError,
  FileWriteError,
  UnknownError,
};

inline bool operator<(ErrorCode a, ErrorCode b) noexcept { return std::to_underlying(a) < std::to_underlying(b); }

struct LinksResult {
  std::vector<std::string> links;
  std::vector<ErrorCode> errors;
};

using FileResult = std::expected<std::filesystem::path, ErrorCode>;

inline std::string_view ErrorCodeToString(ErrorCode code) {
  switch (code) {
    case ErrorCode::HttpError:
      return "Http Error";
    case ErrorCode::ParseError:
      return "Parse Error";
    case ErrorCode::ServiceUnavailable:
      return "Service Unavailable";
    case ErrorCode::AccessDenied:
      return "Access Denied";
    case ErrorCode::ScriptError:
      return "Script Error";
    case ErrorCode::FileWriteError:
      return "File Write Error";
    case ErrorCode::UnknownError:
      return "Unknown Error";
  }
  std::unreachable();
}

inline std::string FormatErrors(const std::vector<ErrorCode>& errors) {
  std::map<ErrorCode, size_t> counts;
  for (auto e : errors) {
    ++counts[e];
  }

  std::string result;
  for (auto it = counts.begin(); it != counts.end(); ++it) {
    if (!result.empty()) {
      result += ", ";
    }
    result += std::format("{} ({})", ErrorCodeToString(it->first), it->second);
  }
  return result;
}

}  // namespace cielparser
