#pragma once

#include <filesystem>
#include <format>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "quill.hpp"
#include "utils.hpp"

namespace cielparser {

class XHS {
  inline static const std::regex url_pattern{R"(https?://(?:www\.)?(?:xiaohongshu|xhslink)\.com/[\w\-./?=&%]+)"};

 public:
  static constexpr std::string_view NAME = "XHS";

  static std::vector<std::string> GetUrls(const std::string_view message) {
    return GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::vector<std::string> res;

    try {
      auto r = HttpGet(url, {{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/121.0.0.0 Safari/537.36"},
                             {"Referer", "https://www.xiaohongshu.com/"}});
      if (!r) {
        return res;
      }

      const size_t start = r->text.find("window.__INITIAL_STATE__=");
      if (start == std::string::npos) {
        LOG_ERROR("Could not find window.__INITIAL_STATE__= in url {}", url);
        return res;
      }

      const size_t json_start = r->text.find('{', start);
      if (json_start == std::string::npos) {
        return res;
      }

      size_t json_end = json_start;
      for (int balance = 0; json_end < r->text.size(); ++json_end) {
        if (r->text[json_end] == '{') {
          ++balance;
        } else if (r->text[json_end] == '}') {
          --balance;
        }
        if (balance == 0) {
          break;
        }
      }

      if (json_end == r->text.size()) {
        LOG_ERROR("Unbalanced braces in JSON state");
        return res;
      }

      const std::string raw_json = SanitizeJson(r->text.substr(json_start, json_end - json_start + 1));
      auto data = nlohmann::json::parse(raw_json);

      const std::string nid = data["note"]["firstNoteId"];
      if (nid.empty()) {
        LOG_ERROR("Note ID is empty, note may not exist or requires login");
        return res;
      }

      const auto& note_data = data["note"]["noteDetailMap"][nid]["note"];

      auto extract_video = [](const nlohmann::json& stream) -> std::string {
        for (const char* codec : {"h264", "h265", "av1"}) {
          if (stream.contains(codec) && stream[codec].is_array() && !stream[codec].empty()) {
            return stream[codec][0].value("masterUrl", "");
          }
        }
        return "";
      };

      if (note_data.value("type", "") == "video" && note_data.contains("video")) {
        if (std::string video_url = extract_video(note_data["video"]["media"]["stream"]); !video_url.empty()) {
          res.emplace_back(std::move(video_url));
          return res;
        }
      }

      if (!note_data.contains("imageList") || !note_data["imageList"].is_array()) {
        return res;
      }

      for (const auto& item : note_data["imageList"]) {
        if (item.contains("stream")) {
          if (std::string live_video_url = extract_video(item["stream"]); !live_video_url.empty()) {
            res.emplace_back(std::move(live_video_url));
          }
        }

        const std::string raw_url = item.value("urlPre", item.value("urlDefault", ""));
        if (raw_url.empty()) {
          LOG_WARNING("raw_url not found in image item");
          continue;
        }

        if (std::string key = ExtractImageKey(raw_url); !key.empty()) {
          res.emplace_back(std::format("https://ci.xiaohongshu.com/{}", key));
        }
      }
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to get download links for {}: {}", url, e.what());
    }

    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(const std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    auto r = HttpGet(download_link);
    if (!r) {
      return std::nullopt;
    }

    if (r->header["Content-Type"].contains("image/")) {
      if (auto r_png = HttpGet(std::string{download_link} += "?imageView2/format/png")) {
        r = std::move(r_png);
      }
    }

    std::string ext = ".bin";
    for (const auto& [type, suffix] : mime_map) {
      if (r->header["Content-Type"].find(type) != std::string::npos) {
        ext = suffix;
        break;
      }
    }

    return SaveContents(download_dir, ext, download_link, r->text);
  }

 private:
  inline static const std::map<std::string, std::string> mime_map = {{"image/jpeg", ".jpeg"},
                                                                     {"image/png", ".png"},
                                                                     {"image/webp", ".webp"},
                                                                     {"video/mp4", ".mp4"},
                                                                     {"binary/octet-stream", ".mp4"}};

  static std::string ExtractImageKey(const std::string& url) {
    static const std::regex pattern(R"(\/[0-9a-f]{32}\/(.+?)!)");
    if (std::smatch match; std::regex_search(url, match, pattern) && match.size() > 1) {
      return match[1].str();
    }
    return "";
  }

  static std::string SanitizeJson(const std::string& raw) {
    static const std::regex undefined_pattern(R"(:undefined)");
    return std::regex_replace(raw, undefined_pattern, ":null");
  }
};

}  // namespace cielparser
