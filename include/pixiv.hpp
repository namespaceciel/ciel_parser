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

class Pixiv {
  inline static const std::regex url_pattern{R"(https?://(?:www\.)?pixiv\.net/artworks/\d+)"};

 public:
  static constexpr std::string_view NAME = "Pixiv";

  static std::vector<std::string> GetUrls(const std::string_view message) {
    return GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::vector<std::string> res;

    try {
      const std::regex re(R"(artworks/(\d+))");
      std::cmatch m;
      if (!std::regex_search(url.begin(), url.end(), m, re)) {
        LOG_ERROR("regex_search failed, url: {} ", url);
        return res;
      }

      const auto r = HttpGet(std::format("https://www.pixiv.net/ajax/illust/{}/pages", m[1].str()),
                             {{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.pixiv.net/"}});
      if (!r) {
        return res;
      }

      for (const auto json = nlohmann::json::parse(r->text); const auto& item : json["body"]) {
        res.emplace_back(item["urls"]["original"].get<std::string>());
      }
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to get download links for {}: {}", url, e.what());
    }

    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(const std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    const auto r = HttpGet(download_link, {{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.pixiv.net/"}});
    if (!r) {
      return std::nullopt;
    }

    const auto ext = std::filesystem::path(download_link).extension().string();
    return SaveContents(download_dir, ext, download_link, r->text);
  }
};

}  // namespace cielparser
