#pragma once

#include <cpr/cpr.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
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
  static std::vector<std::string> GetUrls(const std::string_view message) {
    return cielparser::GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static std::vector<std::string> GetDownloadLinks(const std::string_view url) {
    std::vector<std::string> res;

    const auto r =
        cpr::Get(cpr::Url{url},
                 cpr::Header{{"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/121.0.0.0 Safari/537.36"},
                             {"Referer", "https://www.xiaohongshu.com/"}});

    if (r.status_code != 200) {
      LOG_ERROR("cpr {} failed, status_code = {}", url, r.status_code);
      return res;
    }

    size_t start = r.text.find("window.__INITIAL_STATE__=");
    if (start == std::string::npos) {
      LOG_ERROR(R"(Could not find "window.__INITIAL_STATE__=" in url {})", url);
      return res;
    }

    start = r.text.find('{', start);
    int balance = 0;
    size_t end = start;
    for (; end < r.text.size(); ++end) {
      if (r.text[end] == '{') {
        balance++;
      } else if (r.text[end] == '}') {
        balance--;
      }
      if (balance == 0 && end > start) {
        break;
      }
    }

    try {
      nlohmann::json data = nlohmann::json::parse(SanitizeJson(r.text.substr(start, end - start + 1)));

      if (!data.contains("note") || !data["note"].contains("firstNoteId")) {
        LOG_ERROR(R"(Could not find data["note"]["firstNoteId"] in url)");
        return res;
      }
      const std::string nid = data["note"]["firstNoteId"];

      if (!data["note"]["noteDetailMap"].contains(nid)) {
        LOG_ERROR(R"(Could not find data["note"]["noteDetailMap"][{}])", nid);
        return res;
      }

      const auto& note_data = data["note"]["noteDetailMap"][nid]["note"];

      const auto extract_video = [](const nlohmann::json& stream) -> std::string {
        if (!stream.is_object()) {
          return "";
        }
        for (const char* codec : {"h264", "h265", "av1"}) {
          if (stream.contains(codec) && stream[codec].is_array() && !stream[codec].empty()) {
            return stream[codec][0].value("masterUrl", "");
          }
        }
        return "";
      };

      if (note_data.value("type", "") == "video" && note_data.contains("video")) {
        if (note_data["video"].contains("media") && note_data["video"]["media"].contains("stream")) {
          if (std::string video_url = extract_video(note_data["video"]["media"]["stream"]); !video_url.empty()) {
            res.emplace_back(std::move(video_url));
            return res;
          }
        }
      }

      if (note_data.contains("imageList") && note_data["imageList"].is_array()) {
        for (const auto& item : note_data["imageList"]) {
          if (item.contains("stream")) {
            if (std::string live_video_url = extract_video(item["stream"]); !live_video_url.empty()) {
              res.emplace_back(std::move(live_video_url));
              continue;
            }
          }

          std::string raw_url;
          if (item.contains("urlPre")) {
            raw_url = item["urlPre"];
          } else if (item.contains("urlDefault")) {
            raw_url = item["urlDefault"];
          } else {
            LOG_WARNING("raw_url not found");
            continue;
          }

          if (const std::string key = ExtractImageKey(raw_url); !key.empty()) {
            res.emplace_back(std::format("https://ci.xiaohongshu.com/{}", key));
          }
        }
      }
    } catch (std::exception& e) {
      LOG_ERROR(R"(e.what(): {})", e.what());
    }
    return res;
  }

  static std::optional<std::filesystem::path> DownloadFile(const std::string_view download_link,
                                                           const std::filesystem::path& download_dir) {
    auto r = cpr::Get(cpr::Url{download_link});
    if (r.status_code != 200) {
      LOG_ERROR("Accessing {} failed", download_link);
      return std::nullopt;
    }
    if (r.header["Content-Type"].contains("image/")) {
      if (auto r_png = cpr::Get(cpr::Url{std::string{download_link} += "?imageView2/format/png"});
          r_png.status_code == 200) {
        r = std::move(r_png);
      }
    }

    std::string ext = ".bin";
    for (const auto& [type, suffix] : mime_map) {
      if (r.header["Content-Type"].find(type) != std::string::npos) {
        ext = suffix;
        break;
      }
    }
    return SaveContents(download_dir, ext, download_link, r.text);
  }

 private:
  inline static const std::map<std::string, std::string> mime_map = {
      {"image/jpeg", ".jpeg"}, {"image/png", ".png"}, {"image/webp", ".webp"}, {"video/mp4", ".mp4"}};

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
