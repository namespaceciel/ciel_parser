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

}  // namespace cielparser
