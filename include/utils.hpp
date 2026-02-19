#pragma once

#include <exception>
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
  using sv_token_iterator = std::regex_token_iterator<std::string_view::const_iterator>;
  const std::vector<std::string> urls(sv_token_iterator(message.begin(), message.end(), pattern), sv_token_iterator());
  return urls;
}

inline std::atomic<size_t> g_counter{0};

inline size_t GenerateUniqueId() { return g_counter.fetch_add(1, std::memory_order_relaxed); }

inline std::filesystem::path SaveContents(const std::filesystem::path& download_dir, const std::string_view ext,
                                          const std::string_view download_link, const std::string_view file_contents) {
  while (true) {
    auto filepath = download_dir / std::format("{}{}", cielparser::GenerateUniqueId(), ext);
    if (std::filesystem::exists(filepath)) {
      continue;
    }
    std::ofstream(filepath, std::ios::binary) << file_contents;
    LOG_INFO("Downloaded {} in {}", download_link, filepath.string());
    return filepath;
  }
}

}  // namespace cielparser
