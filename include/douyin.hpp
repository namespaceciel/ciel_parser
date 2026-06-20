#pragma once

#include <array>
#include <filesystem>
#include <format>
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

class DouYin {
  inline static const std::regex url_pattern{R"(https?://v\.douyin\.com/[a-zA-Z0-9_-]+/?)"};

 public:
  static constexpr std::string_view NAME = "DouYin";

  static std::vector<std::string> GetUrls(const std::string_view message) {
    return GetMatchedUrlsFromPattern(message, url_pattern);
  }

  static LinksResult GetDownloadLinks(const std::string_view url) {
    LinksResult result;

    try {
      const std::string python_script =
          std::filesystem::path(__FILE__).parent_path().parent_path() / "scripts" / "douyin_parser.py";
      const std::string cmd = std::format(R"(python3 "{}" "{}" 2>/dev/null)", python_script, url);

      const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
      if (!pipe) {
        LOG_ERROR("Failed to run douyin parser script");
        result.errors.emplace_back(ErrorCode::ScriptError);
        return result;
      }

      std::array<char, 128> buffer{};
      std::string output;
      while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
      }
      if (output.empty()) {
        result.errors.emplace_back(ErrorCode::ScriptError);
        return result;
      }

      if (const auto json = nlohmann::json::parse(output); json.is_array()) {
        for (const auto& item : json) {
          if (item.is_string()) {
            result.links.emplace_back(item.get<std::string>());
          }
        }
      } else if (json.contains("error")) {
        LOG_ERROR(R"(DouYin parser error, json["error"]: {})", json["error"].get<std::string>());
        result.errors.emplace_back(ErrorCode::ScriptError);
      } else {
        LOG_ERROR("DouYin parser error, json: {}", json.get<std::string>());
        result.errors.emplace_back(ErrorCode::ParseError);
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
};

}  // namespace cielparser
