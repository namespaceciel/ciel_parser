#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace cielparser {

struct Config {
  std::string bot_token;
  std::string tg_api_endpoint;  // Use "https://api.telegram.org" when it's empty
  std::filesystem::path download_dir;
  std::filesystem::path log_path;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Config, bot_token, tg_api_endpoint, download_dir, log_path);
};

}  // namespace cielparser
