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

  static std::vector<std::string> GetDownloadLinks(std::string_view url) {
    std::regex re(R"(artworks/(\d+))");
    std::cmatch m;
    if (!std::regex_search(url.begin(), url.end(), m, re)) {
      LOG_ERROR("regex_search failed, url: {} ", url);
      return {};
    }

    auto r = HttpGet(std::format("https://www.pixiv.net/ajax/illust/{}/pages", m[1].str()),
                     {{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.pixiv.net/"}});
    if (!r) {
      return {};
    }

    std::vector<std::string> res;
    for (const auto& item : nlohmann::json::parse(r->text)["body"]) {
      res.emplace_back(item["urls"]["original"].get<std::string>());
    }
    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    auto r = HttpGet(download_link, {{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.pixiv.net/"}});
    if (!r) {
      return std::nullopt;
    }

    auto ext = std::filesystem::path(download_link).extension().string();
    return SaveContents(download_dir, ext, download_link, r->text);
  }
};

}  // namespace cielparser
