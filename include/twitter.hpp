#pragma once

#include <filesystem>
#include <format>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "quill.hpp"
#include "utils.hpp"

namespace cielparser {

class Twitter {
  inline static const std::regex url_pattern{R"((?:https?://)?(?:www\.)?(?:twitter|x)\.com/[^/]+/status/\d+)"};

 public:
  static constexpr std::string_view NAME = "Twitter";

  static std::vector<std::string> GetUrls(const std::string_view message) {
    return GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::string id = std::regex_replace(std::string{url}, std::regex(R"(^.*status/(\d+).*$)"), "$1");
    auto r = HttpGet(std::format("https://api.vxtwitter.com/Twitter/status/{}", id));
    if (!r) {
      return {};
    }

    std::vector<std::string> res;
    if (auto json = nlohmann::json::parse(r->text, nullptr, false); json.contains("media_extended")) {
      for (const auto& media : json["media_extended"]) {
        res.emplace_back(media.value("url", ""));
      }
    }
    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(const std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    const auto url_prefix = download_link.substr(0, download_link.find('?'));
    auto ext = std::filesystem::path(url_prefix).extension().string();
    if (ext.empty()) {
      ext = ".mp4";
    }

    std::string final_download_link{download_link};
    if (download_link.contains("pbs.twimg.com") && (ext == ".jpg" || ext == ".png")) {
      final_download_link =
          std::format("{}?format=png&name=4096x4096", url_prefix.substr(0, url_prefix.length() - ext.length()));
      ext = ".png";
      LOG_INFO("download_link changes from {} to {}", download_link, final_download_link);
    }

    const auto r = HttpGet(final_download_link);
    if (!r) {
      return std::nullopt;
    }
    return SaveContents(download_dir, ext, download_link, r->text);
  }
};

}  // namespace cielparser
