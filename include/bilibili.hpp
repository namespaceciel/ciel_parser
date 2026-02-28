#pragma once

#include <cpr/cpr.h>

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

class Bilibili {
  inline static const std::regex url_pattern{
      R"((?:https?://)?(?:(?:(?:www\.|m\.|t\.)?bilibili\.com/(?:video|opus)/)|(?:b23\.tv|bili2233\.cn)/)[^ \s\u3000]+)"};

 public:
  inline static constexpr std::string_view NAME = "Bilibili";

  static std::vector<std::string> GetUrls(const std::string_view message) {
    return cielparser::GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static std::vector<std::string> GetDownloadLinks(std::string_view url) {
    try {
      cpr::Session session;
      session.SetUrl(cpr::Url{std::string{url}});
      session.SetHeader(cpr::Header{{"User-Agent",
                                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                                     "Chrome/120.0.0.0 Safari/537.36"}});
      auto resp = session.Get();

      std::string final_url = resp.url.str();
      std::regex bv_re(R"(BV[a-zA-Z0-9]{10})");
      std::smatch match;
      if (!std::regex_search(final_url, match, bv_re)) {
        return {};
      }

      std::string bvid = match.str();
      auto view_resp = HttpGet(std::format("https://api.bilibili.com/x/web-interface/view?bvid={}", bvid));
      if (!view_resp) {
        return {};
      }

      std::vector<std::string> res;
      auto view_json = nlohmann::json::parse(view_resp->text);
      if (!view_json.contains("data") || !view_json["data"].contains("pages")) {
        return {};
      }

      for (const auto& page : view_json["data"]["pages"]) {
        if (page.value("duration", 0) > 600) {
          LOG_WARNING("duration > 600s, skip");
          continue;
        }

        auto cid = page["cid"].get<uint64_t>();
        auto play_resp =
            HttpGet(std::format("https://api.bilibili.com/x/player/playurl?bvid={}&cid={}&qn=120", bvid, cid),
                    {{"Referer", "https://www.bilibili.com"}, {"User-Agent", "Mozilla/5.0"}});
        if (!play_resp) {
          continue;
        }

        auto play_json = nlohmann::json::parse(play_resp->text);
        if (!play_json.contains("data") || !play_json["data"].contains("durl")) {
          continue;
        }

        for (const auto& item : play_json["data"]["durl"]) {
          if (item.contains("url")) {
            res.emplace_back(item["url"].get<std::string>());
          }
        }
      }
      return res;
    } catch (const std::exception& e) {
      LOG_ERROR("exception caught: {}", e.what());
    }
    return {};
  }

  static std::optional<std::filesystem::path> DownloadFile(std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    auto r = HttpGet(download_link, {{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.bilibili.com"}});
    if (!r) {
      return std::nullopt;
    }

    auto ext = download_link.substr(download_link.rfind('.'), download_link.find('?') - download_link.rfind('.'));
    return SaveContents(download_dir, ext, download_link, r->text);
  }
};

}  // namespace cielparser
