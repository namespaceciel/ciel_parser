#pragma once

#include <array>
#include <filesystem>
#include <format>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "quill.hpp"
#include "utils.hpp"

namespace cielparser {

class DouYin {
  inline static const std::regex url_pattern{R"(https?://v\.douyin\.com/[a-zA-Z0-9_-]+/?)"};

 public:
  static constexpr std::string_view NAME = "DouYin";

  static std::vector<std::string> GetUrls(const std::string_view message) {
    return GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::vector<std::string> res;

    const std::string python_script =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "scripts" / "douyin_parser.py";
    const std::string cmd = std::format("python3 \"{}\" \"{}\" 2>/dev/null", python_script, url);

    std::array<char, 128> buffer;
    std::string output;
    const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
      LOG_ERROR("Failed to run douyin parser script");
      return res;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      output += buffer.data();
    }

    if (output.empty()) {
      return res;
    }

    try {
      if (const auto json = nlohmann::json::parse(output); json.is_array()) {
        for (const auto& item : json) {
          if (item.is_string()) {
            res.emplace_back(item.get<std::string>());
          }
        }
      } else if (json.contains("error")) {
        LOG_ERROR("DouYin parser error: {}", json["error"].get<std::string>());
      }
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to parse douyin parser output: {}", e.what());
    }

    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(const std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    auto r = HttpGet(download_link, {{"Referer", "https://www.douyin.com/"}});
    if (!r) {
      return std::nullopt;
    }

    std::string ext = ".bin";
    if (r->header["Content-Type"].contains("video")) {
      ext = ".mp4";
    } else if (r->header["Content-Type"].contains("image")) {
      ext = ".jpeg";
    }
    return SaveContents(download_dir, ext, download_link, r->text);
  }
};

}  // namespace cielparser
