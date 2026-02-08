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

class WeiBo {
 public:
  static std::vector<std::string> GetUrls(const std::string_view message) {
    static const std::regex pattern(R"((https?://(?:m\.)?weibo\.(?:com|cn)/(?!u/)[^\s]+))");
    using sv_token_iterator = std::regex_token_iterator<std::string_view::const_iterator>;
    const std::vector<std::string> urls(sv_token_iterator(message.begin(), message.end(), pattern),
                                        sv_token_iterator());
    return urls;
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::vector<std::string> res;

    std::string id(url.substr(url.find_last_of('/') + 1));
    if (const size_t q_pos = id.find('?'); q_pos != std::string::npos) {
      id.resize(q_pos);
    }

    cpr::Response r = cpr::Get(
        cpr::Url{"https://weibo.com/ajax/statuses/show"}, cpr::Parameters{{"id", id}},
        cpr::Header{
            {"Cookie",
             "SUB=_2AkMR47Mlf8NxqwFRmfocxG_lbox2wg7EieKnv0L-JRMxHRl-yT9yqhFdtRB6OmOdyoia9pKPkqoHRRmSBA_WNPaHuybH"},
            {"User-Agent",
             "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 "
             "Safari/537.36"},
            {"Referer", std::string{url}},
            {"X-Requested-With", "XMLHttpRequest"},
            {"Accept", "application/json, text/plain, */*"},
            {"Client-Version", "v2.44.0"}});

    if (r.status_code != 200) {
      LOG_ERROR("cpr {} failed, status_code = {}", url, r.status_code);
      return res;
    }

    auto json = nlohmann::json::parse(r.text, nullptr, false);
    if (json.is_discarded()) {
      LOG_ERROR("json.is_discarded(), r.text: {}", r.text);
      return res;
    }

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

    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(const std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    const auto r = cpr::Get(cpr::Url{download_link});
    if (r.status_code != 200) {
      LOG_ERROR("Accessing {} failed", download_link);
      return std::nullopt;
    }

    const auto url_str = download_link.substr(0, download_link.find('?'));
    auto ext = std::filesystem::path(url_str).extension().string();
    if (ext.empty()) {
      ext = ".bin";
    }

    const auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    auto filepath = download_dir / std::format("{}{}", ts, ext);
    std::ofstream(filepath, std::ios::binary) << r.text;
    LOG_INFO("Downloaded {} in {}", download_link, filepath.string());
    return std::make_optional(std::move(filepath));
  }
};

}  // namespace cielparser
