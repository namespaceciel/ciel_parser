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

  static LinksResult GetDownloadLinks(const std::string_view url) {
    LinksResult result;

    try {
      const std::string id = std::regex_replace(std::string{url}, std::regex(R"(^.*status/(\d+).*$)"), "$1");
      const auto r = HttpGet(std::format("https://api.vxtwitter.com/Twitter/status/{}", id));
      if (!r || r->text.empty()) {
        result.errors.emplace_back(ErrorCode::HttpError);
        return result;
      }

      if (r->text[0] == '<') {
        LOG_ERROR("vxtwitter API returned HTML instead of JSON for ID {}. The service might be down or blocking.", id);
        result.errors.emplace_back(ErrorCode::AccessDenied);
        return result;
      }

      if (const auto json = nlohmann::json::parse(r->text); json.contains("media_extended")) {
        for (const auto& media : json["media_extended"]) {
          result.links.emplace_back(media["url"].get<std::string>());
        }
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
      return std::unexpected(ErrorCode::HttpError);
    }
    return SaveContents(download_dir, ext, final_download_link, r->text);
  }
};

}  // namespace cielparser
