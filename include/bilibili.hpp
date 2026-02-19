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

class Bilibili {
  inline static const std::regex url_pattern{
      R"((?:https?://)?(?:(?:(?:www\.|m\.|t\.)?bilibili\.com/(?:video|opus)/)|(?:b23\.tv|bili2233\.cn)/)[^ \s\u3000]+)"};

 public:
  static std::vector<std::string> GetUrls(const std::string_view message) {
    return cielparser::GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::vector<std::string> res;
    try {
      cpr::Session session;
      session.SetUrl(cpr::Url{url});
      session.SetHeader(cpr::Header{{"User-Agent",
                                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                                     "Chrome/120.0.0.0 Safari/537.36"}});
      auto resp = session.Get();

      const std::string final_url = resp.url.str();
      const std::regex bv_re(R"(BV[a-zA-Z0-9]{10})");
      std::smatch match;
      if (!std::regex_search(final_url, match, bv_re)) {
        return res;
      }

      const std::string bvid = match.str();
      const auto view_resp =
          cpr::Get(cpr::Url{std::format("https://api.bilibili.com/x/web-interface/view?bvid={}", bvid)}).text;

      if (const auto view_json = nlohmann::json::parse(view_resp); view_json["data"].contains("pages")) {
        for (const auto& page : view_json["data"]["pages"]) {
          const auto cid = page["cid"].get<uint64_t>();
          const auto play_resp =
              cpr::Get(
                  cpr::Url{std::format("https://api.bilibili.com/x/player/playurl?bvid={}&cid={}&qn=120", bvid, cid)},
                  cpr::Header{{"Referer", "https://www.bilibili.com"}, {"User-Agent", "Mozilla/5.0"}})
                  .text;
          if (const auto play_json = nlohmann::json::parse(play_resp); play_json["data"].contains("durl")) {
            for (const auto& item : play_json["data"]["durl"]) {
              if (item.contains("url")) {
                res.emplace_back(item["url"].get<std::string>());
              }
            }
          }
        }
      }
    } catch (const std::exception& e) {
      LOG_ERROR("exception caught: {}", e.what());
    }
    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(const std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    const auto r = cpr::Get(cpr::Url{download_link},
                            cpr::Header{{"User-Agent", "Mozilla/5.0"}, {"Referer", "https://www.bilibili.com"}});
    if (r.status_code != 200) {
      LOG_ERROR("Accessing {} failed", download_link);
      return std::nullopt;
    }

    const auto ext = download_link.substr(download_link.rfind('.'), download_link.find('?') - download_link.rfind('.'));
    return SaveContents(download_dir, ext, download_link, r.text);
  }
};

}  // namespace cielparser
