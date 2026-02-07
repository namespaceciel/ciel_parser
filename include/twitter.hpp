#pragma once

#include <cpr/cpr.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "quill.hpp"

namespace cielparser {

class Twitter {
 public:
  static std::vector<std::string> GetUrls(const std::string& message) {
    static const std::regex pattern(R"((?:https?://)?(?:www\.)?(?:twitter|x)\.com/[^/]+/status/\d+)");
    const std::vector<std::string> urls(std::sregex_token_iterator(message.begin(), message.end(), pattern, 0),
                                        std::sregex_token_iterator());
    return urls;
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::vector<std::string> res;

    const std::string id = std::regex_replace(std::string{url}, std::regex(R"(^.*status/(\d+).*$)"), "$1");
    const auto r = cpr::Get(cpr::Url{std::format("https://api.vxtwitter.com/Twitter/status/{}", id)});
    if (r.status_code != 200) {
      LOG_ERROR("cpr {} failed, status_code = {}", url, r.status_code);
      return res;
    }

    if (auto json = nlohmann::json::parse(r.text, nullptr, false); json.contains("media_extended")) {
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
      final_download_link = std::format("{}?format={}&name=orig",
                                        url_prefix.substr(0, url_prefix.length() - ext.length()), ext.substr(1));
      LOG_INFO("download_link changes from {} to {}", download_link, final_download_link);
    }

    const auto r = cpr::Get(cpr::Url{final_download_link});
    if (r.status_code != 200) {
      LOG_ERROR("Accessing {} failed", final_download_link);
      return std::nullopt;
    }

    const auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    auto filepath = download_dir / std::format("{}{}", ts, ext);
    std::ofstream(filepath, std::ios::binary) << r.text;
    LOG_INFO("Downloaded {} in {}", final_download_link, filepath.string());
    return std::make_optional(std::move(filepath));
  }
};

}  // namespace cielparser
