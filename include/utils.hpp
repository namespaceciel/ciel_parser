#pragma once

#include <cpr/cpr.h>
#define MINIMP4_IMPLEMENTATION
#include <minimp4.h>

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
  cpr::Response r = cpr::Get(cpr::Url{url}, params, headers);
  if (r.status_code != 200) {
    LOG_ERROR("Download {} failed, status_code = {}", url, r.status_code);
    return std::nullopt;
  }
  return r;
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

inline VideoInfo GetVideoInfo(const std::filesystem::path& file_path) {
  auto Mp4ReadCallback = [](const int64_t offset, void* buffer, const size_t size, void* token) -> int {
    auto* f = static_cast<std::ifstream*>(token);
    f->seekg(offset, std::ios::beg);
    if (!f->read(static_cast<char*>(buffer), size)) {
      return f->gcount() != size;
    }
    return 0;
  };

  VideoInfo info;
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file) {
    return info;
  }

  const auto file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  MP4D_demux_t mp4{};
  if (MP4D_open(&mp4, Mp4ReadCallback, &file, file_size) == 1) {
    if (mp4.timescale > 0) {
      const uint64_t duration = (static_cast<uint64_t>(mp4.duration_hi) << 32) | mp4.duration_lo;
      info.duration = static_cast<int>(duration / mp4.timescale);
    }
    for (unsigned i = 0; i < mp4.track_count; ++i) {
      if (mp4.track[i].handler_type == MP4D_HANDLER_TYPE_VIDE) {
        info.width = mp4.track[i].SampleDescription.video.width;
        info.height = mp4.track[i].SampleDescription.video.height;
        break;
      }
    }
    MP4D_close(&mp4);
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
