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
#include "utils.hpp"

namespace cielparser {

class Pixiv {
  inline static const std::regex url_pattern{R"(https?://(?:www\.)?pixiv\.net/artworks/\d+)"};

 public:
  static std::vector<std::string> GetUrls(const std::string_view message) {
    return cielparser::GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::vector<std::string> res;

    const std::regex re(R"(artworks/(\d+))");
    std::cmatch m;
    if (!std::regex_search(url.begin(), url.end(), m, re)) {
      LOG_ERROR("regex_search failed, url: {} ", url);
      return res;
    }

    const auto r = cpr::Get(cpr::Url{std::format("https://www.pixiv.net/ajax/illust/{}/pages", m[1].str())},
                            cpr::Header{{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.pixiv.net/"}});

    if (r.status_code != 200) {
      LOG_ERROR("cpr {} failed, status_code = {}", url, r.status_code);
      return res;
    }

    for (const auto& item : nlohmann::json::parse(r.text)["body"]) {
      res.emplace_back(item["urls"]["original"].get<std::string>());
    }
    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(const std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    const auto r = cpr::Get(cpr::Url{download_link},
                            cpr::Header{{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.pixiv.net/"}});
    if (r.status_code != 200) {
      LOG_ERROR("Accessing {} failed", download_link);
      return std::nullopt;
    }

    const auto ext = std::filesystem::path(download_link).extension().string();
    return SaveContents(download_dir, ext, download_link, r.text);
  }
};

}  // namespace cielparser
