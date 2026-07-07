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

  static LinksResult GetDownloadLinks(const std::string_view url) {
    LinksResult result;

    try {
      std::string page_url(url);
      if (page_url.find("xhslink.com") != std::string::npos) {
        page_url = ResolveShortLink(page_url);
        if (page_url.empty()) {
          result.errors.emplace_back(ErrorCode::HttpError);
          return result;
        }
      }

      static const std::regex note_id_re(R"(/(?:explore|discovery/item)/([0-9a-zA-Z]+))");
      std::string note_id;
      if (std::smatch m; std::regex_search(page_url, m, note_id_re) && m.size() > 1) {
        note_id = m[1].str();
      }
      if (note_id.empty()) {
        LOG_ERROR("Could not extract note_id from URL: {}", page_url);
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      std::string query_string;
      if (size_t q = page_url.find('?'); q != std::string::npos) {
        query_string = page_url.substr(q);
      }

      static const cpr::Header desktop_headers{
          {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/121.0.0.0 Safari/537.36"},
          {"Referer", "https://www.xiaohongshu.com/"},
          {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"},
      };

      static const cpr::Header mobile_headers{
          {"User-Agent",
           "Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) "
           "Version/16.6 Mobile/15E148 Safari/604.1"},
          {"origin", "https://www.xiaohongshu.com"},
          {"x-requested-with", "XMLHttpRequest"},
          {"sec-fetch-site", "same-origin"},
          {"sec-fetch-mode", "cors"},
          {"sec-fetch-dest", "empty"},
      };

      const bool is_explore = page_url.find("/explore/") != std::string::npos;
      const std::string fetch_url =
          is_explore ? std::string{page_url}
                     : std::format("https://www.xiaohongshu.com/discovery/item/{}{}", note_id, query_string);
      const cpr::Header& fetch_headers = is_explore ? desktop_headers : mobile_headers;

      cpr::Response r = cpr::Get(cpr::Url{fetch_url}, fetch_headers, cpr::Timeout{15000});
      if (r.status_code != 200) {
        LOG_ERROR("Failed to fetch {}: status={}", fetch_url, r.status_code);
        result.errors.emplace_back(ErrorCode::HttpError);
        return result;
      }

      const size_t start = r.text.find("window.__INITIAL_STATE__=");
      if (start == std::string::npos) {
        LOG_ERROR("Could not find window.__INITIAL_STATE__= in url {}", fetch_url);
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      const size_t json_start = r.text.find('{', start);
      if (json_start == std::string::npos) {
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      const size_t json_end = r.text.find("</script>", json_start);
      if (json_end == std::string::npos) {
        LOG_ERROR("Could not find </script> after __INITIAL_STATE__");
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      const std::string raw_json = SanitizeJson(r.text.substr(json_start, json_end - json_start));
      const auto data = nlohmann::json::parse(raw_json);

      const auto& note_data = [&]() -> const nlohmann::json& {
        static const nlohmann::json null_json;
        if (data.contains("noteData") && data["noteData"].contains("data") &&
            data["noteData"]["data"].contains("noteData")) {
          return data["noteData"]["data"]["noteData"];
        }
        if (data.contains("noteData") && data["noteData"].contains("title")) {
          return data["noteData"];
        }
        if (data.contains("note") && data["note"].contains("noteDetailMap")) {
          const std::string nid = data["note"].value("firstNoteId", "");
          if (!nid.empty() && data["note"]["noteDetailMap"].contains(nid)) {
            return data["note"]["noteDetailMap"][nid]["note"];
          }
        }
        return null_json;
      }();

      if (!note_data.is_object()) {
        LOG_ERROR("Note data not found, note may be deleted or private");
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      const auto extract_video = [](const nlohmann::json& stream) -> std::string {
        for (const char* codec : {"h265", "h264", "av1"}) {
          if (stream.contains(codec) && stream[codec].is_array() && !stream[codec].empty()) {
            return stream[codec][0].value("masterUrl", "");
          }
        }
        return "";
      };

      if (note_data.value("type", "") == "video" && note_data.contains("video")) {
        if (std::string video_url = extract_video(note_data["video"]["media"]["stream"]); !video_url.empty()) {
          result.links.emplace_back(std::move(video_url));
        }
        if (note_data.contains("cover") && note_data["cover"].contains("fileId")) {
          const std::string cover_id = note_data["cover"]["fileId"].get<std::string>();
          if (!cover_id.empty()) {
            result.links.emplace_back(std::format("https://ci.xiaohongshu.com/{}", cover_id));
          }
        }
        if (!result.links.empty()) {
          return result;
        }
      }

      if (!note_data.contains("imageList") || !note_data["imageList"].is_array()) {
        LOG_ERROR("Note has no imageList or imageList is not an array");
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      for (const auto& item : note_data["imageList"]) {
        if (item.contains("stream")) {
          if (std::string live_video_url = extract_video(item["stream"]); !live_video_url.empty()) {
            result.links.emplace_back(std::move(live_video_url));
          }
        }

        const std::string raw_url = item.value("urlPre", item.value("urlDefault", item.value("url", "")));
        if (raw_url.empty()) {
          LOG_WARNING("raw_url not found in image item");
          result.errors.emplace_back(ErrorCode::ParseError);
          continue;
        }

        if (const std::string key = ExtractImageKey(raw_url); !key.empty()) {
          result.links.emplace_back(std::format("https://ci.xiaohongshu.com/{}", key));
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
    const bool is_ci_image = download_link.find("ci.xiaohongshu.com") != std::string_view::npos;
    auto r = is_ci_image ? HttpGet(std::string{download_link} + "?imageView2/format/png") : std::nullopt;
    if (!r) {
      r = HttpGet(download_link);
    }
    if (!r) {
      return std::unexpected(ErrorCode::HttpError);
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

  static std::string ResolveShortLink(const std::string& short_url) {
    static const cpr::Header mobile_headers{
        {"User-Agent",
         "Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) "
         "Version/16.6 Mobile/15E148 Safari/604.1"},
        {"origin", "https://www.xiaohongshu.com"},
        {"x-requested-with", "XMLHttpRequest"},
    };

    cpr::Response r = cpr::Get(cpr::Url{short_url}, mobile_headers, cpr::Redirect{false}, cpr::Timeout{15000});
    if (r.status_code >= 300 && r.status_code < 400 && !r.header["Location"].empty()) {
      LOG_INFO("XHS short link resolved: {} -> {}", short_url, r.header["Location"]);
      return r.header["Location"];
    }
    LOG_ERROR("XHS short link resolution failed: status={}", r.status_code);
    return "";
  }

  static std::string ExtractImageKey(const std::string& url) {
    static const std::regex pattern(R"(\/[0-9a-f]{32}\/(.+?)!)");
    if (std::smatch match; std::regex_search(url, match, pattern) && match.size() > 1) {
      return match[1].str();
    }

    static const std::regex fileid_pattern(R"(notes_pre_post/(.+))");
    if (std::smatch match; std::regex_search(url, match, fileid_pattern) && match.size() > 1) {
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
