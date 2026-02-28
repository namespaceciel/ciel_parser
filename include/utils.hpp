#pragma once

#include <cpr/cpr.h>

#include <chrono>
#include <exception>
#include <optional>
#include <random>
#include <thread>

#include "quill.hpp"

namespace cielparser {

template <size_t N>
void TryNTimes(auto&& f) {
  for (size_t i = 0; i < N; ++i) {
    try {
      f();
      return;
    } catch (const std::exception& e) {
      LOG_ERROR("exception caught: {}, sleep for 5 second and retry", e.what());
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }
}

inline std::vector<std::string> GetMatchedUrlsFromPattern(const std::string_view message, const std::regex& pattern) {
  using Iterator = std::regex_token_iterator<std::string_view::const_iterator>;
  return {Iterator{message.begin(), message.end(), pattern}, Iterator{}};
}

inline std::optional<cpr::Response> HttpGet(std::string_view url, const cpr::Header& headers = {},
                                            const cpr::Parameters& params = {}) {
  cpr::Response r = cpr::Get(cpr::Url{std::string{url}}, params, headers);
  if (r.status_code != 200) {
    LOG_ERROR("HTTP GET {} failed, status_code = {}", url, r.status_code);
    return std::nullopt;
  }
  return r;
}

inline std::filesystem::path SaveContents(const std::filesystem::path& download_dir, std::string_view ext,
                                          std::string_view download_link, std::string_view file_contents) {
  static std::atomic<uint64_t> counter{0};
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const uint64_t unique_id = now + counter.fetch_add(1, std::memory_order_relaxed);

  auto filepath = download_dir / std::format("{}{}", unique_id, ext);
  std::ofstream(filepath, std::ios::binary) << file_contents;
  LOG_INFO("Downloaded {} in {}", download_link, filepath.string());
  return filepath;
}

}  // namespace cielparser
