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

}  // namespace cielparser
