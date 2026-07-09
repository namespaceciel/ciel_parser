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

  inline static std::string cookie;

  static std::vector<std::string> GetUrls(const std::string_view message) {
    return GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static LinksResult GetDownloadLinks(const std::string_view url) {
    LinksResult result;

    try {
      const std::regex re(R"(artworks/(\d+))");
      std::cmatch m;
      if (!std::regex_search(url.begin(), url.end(), m, re)) {
        LOG_ERROR("regex_search failed, url: {} ", url);
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      const std::string art_id = m[1].str();

      cpr::Header headers{{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.pixiv.net/"}};
      if (!cookie.empty()) {
        headers["Cookie"] = cookie;
      }

      const auto r = HttpGet(std::format("https://www.pixiv.net/ajax/illust/{}/pages", art_id), headers);

      if (!r) {
        if (auto detail = HttpGet(std::format("https://www.pixiv.net/ajax/illust/{}", art_id), headers)) {
          if (const auto d = nlohmann::json::parse(detail->text); d["body"].value("xRestrict", 0) > 0) {
            LOG_ERROR("Pixiv artwork {} requires login (R-18 restricted)", art_id);
            result.errors.emplace_back(ErrorCode::AccessDenied);
            return result;
          }
        }
        result.errors.emplace_back(ErrorCode::HttpError);
        return result;
      }

      for (const auto json = nlohmann::json::parse(r->text); const auto& item : json["body"]) {
        result.links.emplace_back(item["urls"]["original"].get<std::string>());
      }
    } catch (const nlohmann::json::parse_error& e) {
      LOG_ERROR("Failed to get download links for {}: {}", url, e.what());
      result.errors.emplace_back(ErrorCode::ParseError);
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to get download links for {}: {}", url, e.what());
      result.errors.emplace_back(ErrorCode::UnknownError);
    }

    return result;
  }

  static FileResult DownloadFile(const std::string_view download_link, const std::filesystem::path& download_dir) {
    const auto r = HttpGet(download_link, {{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.pixiv.net/"}});
    if (!r) {
      return std::unexpected(ErrorCode::HttpError);
    }

    const auto ext = std::filesystem::path(download_link).extension().string();
    return SaveContents(download_dir, ext, download_link, r->text);
  }
};

}  // namespace cielparser
