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

class DouYin {
  inline static const std::regex short_pattern{R"(https?://v\.douyin\.com/[a-zA-Z0-9_-]+/?)"};
  inline static const std::regex long_pattern{R"(https?://(?:www\.)?douyin\.com/(?:video|note)/\d+)"};
  inline static const std::regex ies_pattern{R"(https?://(?:www\.)?iesdouyin\.com/share/(?:video|note|slides)/\d+)"};
  inline static const std::regex m_pattern{R"(https?://m\.douyin\.com/share/(?:video|note|slides)/\d+)"};

 public:
  static constexpr std::string_view NAME = "DouYin";

  static std::vector<std::string> GetUrls(const std::string_view message) {
    std::vector<std::string> urls;

    for (const std::regex* p : {&short_pattern, &long_pattern, &ies_pattern, &m_pattern}) {
      for (auto it = std::regex_token_iterator<std::string_view::const_iterator>(message.begin(), message.end(), *p);
           it != std::regex_token_iterator<std::string_view::const_iterator>{}; ++it) {
        urls.emplace_back(*it);
      }
    }
    return urls;
  }

  static LinksResult GetDownloadLinks(const std::string_view url) {
    LinksResult result;

    try {
      std::string page_url(url);
      if (!page_url.starts_with("http")) {
        page_url = "https://" + page_url;
      }

      if (std::regex_search(page_url, short_pattern)) {
        page_url = ResolveShortLink(page_url);
        if (page_url.empty()) {
          result.errors.emplace_back(ErrorCode::HttpError);
          return result;
        }
      }

      static const std::regex id_re(R"((?:video|note|slides)/(\d+))");
      std::string aweme_id;
      if (std::smatch m; std::regex_search(page_url, m, id_re) && m.size() > 1) {
        aweme_id = m[1].str();
      }
      if (aweme_id.empty()) {
        // Try modal_id query param
        static const std::regex modal_re(R"(modal_id=(\d+))");
        if (std::smatch m; std::regex_search(page_url, m, modal_re) && m.size() > 1) {
          aweme_id = m[1].str();
        }
      }
      if (aweme_id.empty()) {
        LOG_ERROR("Could not extract aweme_id from URL: {}", page_url);
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      const std::string share_url = std::format("https://www.iesdouyin.com/share/video/{}/", aweme_id);

      static const cpr::Header ios_headers{
          {"User-Agent",
           "Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) "
           "Version/16.6 Mobile/15E148 Safari/604.1"},
          {"Referer", "https://www.douyin.com/"},
      };

      cpr::Response r = cpr::Get(cpr::Url{share_url}, ios_headers, cpr::Timeout{15000});
      if (r.status_code != 200) {
        LOG_ERROR("Failed to fetch {}: status={}", share_url, r.status_code);
        result.errors.emplace_back(ErrorCode::HttpError);
        return result;
      }

      static const std::regex router_re(R"(window\._ROUTER_DATA\s*=\s*(.*?)</script>)");
      std::smatch router_match;
      if (!std::regex_search(r.text, router_match, router_re) || router_match.size() < 2) {
        LOG_ERROR("Could not find _ROUTER_DATA in share page for {}", aweme_id);
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      std::string raw_json = router_match[1].str();
      static const std::regex undef_re(R"(:undefined)");
      raw_json = std::regex_replace(raw_json, undef_re, ":null");
      const auto data = nlohmann::json::parse(raw_json);

      const auto& ld = data["loaderData"];
      std::string page_key;
      for (auto it = ld.begin(); it != ld.end(); ++it) {
        if (it.key().contains("page")) {
          page_key = it.key();
          break;
        }
      }
      if (page_key.empty() || !ld[page_key].is_object()) {
        LOG_ERROR("No page data in loaderData for {}", aweme_id);
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      const auto& page = ld[page_key];
      if (!page.contains("videoInfoRes") || !page["videoInfoRes"].is_object()) {
        LOG_ERROR("No videoInfoRes in page data for {}", aweme_id);
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      const auto& vir = page["videoInfoRes"];
      if (!vir.contains("item_list") || !vir["item_list"].is_array() || vir["item_list"].empty()) {
        LOG_ERROR("No item_list in videoInfoRes for {}", aweme_id);
        result.errors.emplace_back(ErrorCode::ParseError);
        return result;
      }

      const auto& item = vir["item_list"][0];

      if (item.contains("video") && item["video"].is_object()) {
        const auto& video = item["video"];
        if (video.value("duration", 0) > 0 && video.contains("play_addr") && video["play_addr"].is_object()) {
          const auto& pa = video["play_addr"];
          if (pa.contains("url_list") && pa["url_list"].is_array() && !pa["url_list"].empty()) {
            std::string video_url = pa["url_list"][0].get<std::string>();
            if (size_t pos = video_url.find("playwm"); pos != std::string::npos) {
              video_url.replace(pos, 6, "play");
            }
            result.links.emplace_back(std::move(video_url));
          }
        }
      }

      if (item.contains("images") && item["images"].is_array()) {
        for (const auto& img : item["images"]) {
          if (!img.is_object() || !img.contains("url_list") || !img["url_list"].is_array() || img["url_list"].empty()) {
            continue;
          }
          result.links.emplace_back(img["url_list"][0].get<std::string>());
        }
      }

    } catch (const nlohmann::json::parse_error& e) {
      LOG_ERROR("Failed to parse douyin data for {}: {}", url, e.what());
      result.errors.emplace_back(ErrorCode::ParseError);
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to get download links for {}: {}", url, e.what());
      result.errors.emplace_back(ErrorCode::UnknownError);
    }

    return result;
  }

  static FileResult DownloadFile(const std::string_view download_link, const std::filesystem::path& download_dir) {
    auto r = HttpGet(download_link, {{"Referer", "https://www.douyin.com/"}});
    if (!r) {
      return std::unexpected(ErrorCode::HttpError);
    }

    std::string ext = ".bin";
    if (r->header["Content-Type"].contains("video")) {
      ext = ".mp4";
    } else if (r->header["Content-Type"].contains("image")) {
      ext = ".jpeg";
    }

    return SaveContents(download_dir, ext, download_link, r->text);
  }

 private:
  static std::string ResolveShortLink(const std::string& short_url) {
    static const cpr::Header ios_headers{
        {"User-Agent",
         "Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) "
         "Version/16.6 Mobile/15E148 Safari/604.1"},
    };

    cpr::Response r = cpr::Get(cpr::Url{short_url}, ios_headers, cpr::Timeout{15000});
    if (r.status_code >= 200 && r.status_code < 400 && !r.url.str().empty()) {
      std::string resolved = r.url.str();
      if (resolved != short_url) {
        LOG_INFO("DouYin short link resolved: {} -> {}", short_url, resolved);
        return resolved;
      }
    }
    LOG_ERROR("DouYin short link resolution failed: status={}", r.status_code);
    return "";
  }
};

}  // namespace cielparser
