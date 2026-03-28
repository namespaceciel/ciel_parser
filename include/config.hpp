#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace cielparser {

struct Config {
  std::string bot_token;
  std::string api_id{};
  std::string api_hash;
  std::string tg_api_http_port{};
  std::filesystem::path download_dir;
  std::filesystem::path log_path;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Config, bot_token, api_id, api_hash, tg_api_http_port, download_dir, log_path);
};

}  // namespace cielparser
