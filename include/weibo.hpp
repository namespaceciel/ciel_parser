#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "quill.hpp"
#include "utils.hpp"

namespace cielparser {

class WeiBo {
  inline static const std::regex url_pattern{R"((https?://(?:m\.)?weibo\.(?:com|cn)/(?!u/)[^\s]+))"};

 public:
  static constexpr std::string_view NAME = "WeiBo";

  static std::vector<std::string> GetUrls(const std::string_view message) {
    return GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::vector<std::string> res;

    try {
      std::string id(url.substr(url.find_last_of('/') + 1));
      if (const size_t q_pos = id.find('?'); q_pos != std::string::npos) {
        id.resize(q_pos);
      }

      const auto r = HttpGet(
          "https://weibo.com/ajax/statuses/show",
          {{"Cookie",
            "SUB=_2AkMR47Mlf8NxqwFRmfocxG_lbox2wg7EieKnv0L-JRMxHRl-yT9yqhFdtRB6OmOdyoia9pKPkqoHRRmSBA_WNPaHuybH"},
           {"User-Agent",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 "
            "Safari/537.36"},
           {"Referer", std::string{url}},
           {"X-Requested-With", "XMLHttpRequest"},
           {"Accept", "application/json, text/plain, */*"},
           {"Client-Version", "v2.44.0"}},
          {{"id", id}});
      if (!r) {
        return res;
      }

      const auto json = nlohmann::json::parse(r->text);

      if (json.contains("pic_ids") && json.contains("pic_infos")) {
        for (const auto& pid : json["pic_ids"]) {
          res.emplace_back(json["pic_infos"][pid.get<std::string>()]["largest"]["url"].get<std::string>());
        }
      }

      if (json.contains("page_info") && json["page_info"].contains("media_info")) {
        if (const auto& media = json["page_info"]["media_info"]; media.contains("playback_list")) {
          const auto& list = media["playback_list"];
          const auto best = std::ranges::max_element(list, [](const auto& a, const auto& b) {
            return a["play_info"].value("size", 0.0) < b["play_info"].value("size", 0.0);
          });
          if (best != list.end()) {
            res.emplace_back((*best)["play_info"]["url"].get<std::string>());
          }
        }
      }
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to get download links for {}: {}", url, e.what());
    }

    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(const std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    const auto r = HttpGet(download_link);
    if (!r) {
      return std::nullopt;
    }

    const auto url_str = download_link.substr(0, download_link.find('?'));
    auto ext = std::filesystem::path(url_str).extension().string();
    if (ext.empty()) {
      ext = ".bin";
    }
    return SaveContents(download_dir, ext, download_link, r->text);
  }
};

}  // namespace cielparser
