#include <algorithm>
#include <filesystem>
#include <future>
#include <optional>
#include <span>
#include <string>
#include <tgbotxx/tgbotxx.hpp>
#include <thread>
#include <tuple>
#include <vector>

#include "bilibili.hpp"
#include "config.hpp"
#include "douyin.hpp"
#include "pixiv.hpp"
#include "quill.hpp"
#include "twitter.hpp"
#include "utils.hpp"
#include "weibo.hpp"
#include "xhs.hpp"

class Bot final : public tgbotxx::Bot {
 public:
  explicit Bot(cielparser::Config config)
      : tgbotxx::Bot(config.bot_token), download_dir_(std::move(config.download_dir)) {
    if (!config.tg_api_endpoint.empty()) {
      api()->setUrl(config.tg_api_endpoint);
    }
  }

  ~Bot() override = default;

  void onStart() override {
    LOG_INFO("Testing API connectivity...");
    const auto me = api()->getMe();
    LOG_INFO("Bot connected: {} (@{})", me->firstName, me->username);
  }

  void onLongPollError(const std::string& errMsg, tgbotxx::ErrorCode errorCode) override {
    LOG_ERROR("Long poll error ({}): {}", static_cast<int>(errorCode), errMsg);
    std::this_thread::sleep_for(std::chrono::minutes(1));
  }

  static constexpr auto kPlatforms = std::tuple<cielparser::XHS, cielparser::WeiBo, cielparser::Twitter,
                                                cielparser::Pixiv, cielparser::Bilibili, cielparser::DouYin>{};

  void onAnyMessage(const tgbotxx::Ptr<tgbotxx::Message>& message) override {
    const auto message_content = !message->text.empty() ? message->text : message->caption;
    if (message_content.empty()) {
      return;
    }

    const auto& user_name =
        message->chat->type == tgbotxx::Chat::Type::Private ? message->from->username : message->chat->username;
    LOG_INFO("Received message {} from @{}", message_content, user_name);

    std::apply([&]<class... Platform>(Platform...) { (ProcessMessage<Platform>(message, message_content), ...); },
               kPlatforms);
  }

 private:
  template <class Platform>
  void ProcessMessage(const tgbotxx::Ptr<tgbotxx::Message>& message, const std::string_view message_content) const {
    for (const auto urls = Platform::GetUrls(message_content); auto&& url : urls) {
      std::thread(&Bot::ProcessUrl<Platform>, this, message, url).detach();
    }
  }

  template <class Platform>
  void ProcessUrl(const tgbotxx::Ptr<tgbotxx::Message> message, const std::string& url) const {
    constexpr auto platform_name = Platform::NAME;

    const std::vector<std::string> download_links = Platform::GetDownloadLinks(url);
    LOG_INFO("Processing URL {} in {}, get {} download_links", url, platform_name, download_links.size());

    std::vector<std::future<std::optional<std::filesystem::path>>> futures;
    futures.reserve(download_links.size());
    for (const auto& link : download_links) {
      futures.emplace_back(std::async(std::launch::async, [this, link] {
        LOG_INFO("Try downloading {}", link);
        return Platform::DownloadFile(link, download_dir_);
      }));
    }

    std::vector<std::filesystem::path> downloaded_files;
    downloaded_files.reserve(download_links.size());
    for (auto& future : futures) {
      if (auto res = future.get(); res.has_value()) {
        downloaded_files.emplace_back(std::move(*res));
      }
    }

    SendDownloadedFiles(message, url, downloaded_files);
  }

  void SendDownloadedFiles(const tgbotxx::Ptr<tgbotxx::Message>& message, const std::string& url,
                           const std::vector<std::filesystem::path>& downloaded_files) const {
    const auto reply_params = cielparser::MakeReplyParameters(message->messageId);

    if (downloaded_files.empty()) {
      cielparser::TryNTimes<3>([&] {
        api()->sendMessage(message->chat->id, std::format("Fail to download files from {}", url), 0, "", {}, false,
                           false, nullptr, "", 0, nullptr, false, "", nullptr, reply_params);
      });
      return;
    }

    std::vector<std::filesystem::path> documents, videos;
    for (const auto& file : downloaded_files) {
      if (file.extension() == ".mp4") {
        videos.emplace_back(file);
      } else {
        documents.emplace_back(file);
      }
    }

    constexpr size_t chunk_size = 10;
    const size_t total_chunks =
        (documents.size() + chunk_size - 1) / chunk_size + (videos.size() + chunk_size - 1) / chunk_size;

    size_t chunk_idx = 0;
    auto send_chunks = [&]<bool IsVideo>(const std::vector<std::filesystem::path>& files) {
      for (size_t i = 0; i < files.size(); i += chunk_size) {
        auto chunk = std::span(files).subspan(i, std::min(chunk_size, files.size() - i));
        const std::string caption = total_chunks > 1
                                        ? std::format("[source]({}) \\[{}/{}\\]", url, ++chunk_idx, total_chunks)
                                        : std::format("[source]({})", url);

        if (chunk.size() == 1) {
          cielparser::TryNTimes<3>([&] {
            if constexpr (IsVideo) {
              const auto info = cielparser::GetVideoInfo(chunk.front());
              api()->sendVideo(message->chat->id, cpr::File(chunk.front().string()), 0, info.duration, info.width,
                               info.height, std::monostate{}, std::monostate{}, 0, caption, "MarkdownV2", {}, false,
                               false, true, false, false, nullptr, "", 0, false, "", nullptr, reply_params);
            } else {
              api()->sendDocument(message->chat->id, cpr::File(chunk.front().string()), 0, std::monostate{}, caption,
                                  "MarkdownV2", {}, false, false, nullptr, "", 0, false, false, "", nullptr,
                                  reply_params);
            }
          });
          continue;
        }

        std::vector<tgbotxx::Ptr<tgbotxx::InputMedia>> media_group;
        media_group.reserve(chunk.size());

        for (const auto& file : chunk) {
          tgbotxx::Ptr<tgbotxx::InputMedia> input_media;
          if constexpr (IsVideo) {
            auto video = std::make_shared<tgbotxx::InputMediaVideo>();
            const auto info = cielparser::GetVideoInfo(file);
            video->width = info.width;
            video->height = info.height;
            video->duration = info.duration;
            video->supportsStreaming = true;
            input_media = std::move(video);
          } else {
            input_media = std::make_shared<tgbotxx::InputMediaDocument>();
          }
          input_media->media = cpr::File(file.string());
          media_group.emplace_back(std::move(input_media));
        }

        media_group.back()->caption = caption;
        media_group.back()->parseMode = "MarkdownV2";

        cielparser::TryNTimes<3>([&] {
          api()->sendMediaGroup(message->chat->id, media_group, 0, false, false, "", 0, false, "", reply_params);
        });
      }
    };

    if (!documents.empty()) {
      send_chunks.operator()<false>(documents);
    }
    if (!videos.empty()) {
      send_chunks.operator()<true>(videos);
    }
  }

  std::filesystem::path download_dir_;
};

int main() {
  const char* config_path = std::getenv("CIELPARSER_CONFIG_PATH");
  if (!config_path) {
    throw std::runtime_error("CIELPARSER_CONFIG_PATH env not set");
  }
  if (!std::filesystem::exists(config_path)) {
    throw std::runtime_error(std::format("Config file {} not found", config_path));
  }
  cielparser::Config config = nlohmann::json::parse(std::ifstream{config_path});
  if (config.bot_token.empty()) {
    throw std::runtime_error("bot_token is empty in config");
  }

  cielparser::SetupQuill(config.log_path);
  std::filesystem::create_directories(config.download_dir);
  Bot bot(std::move(config));
  LOG_INFO("Starting bot...");
  bot.start();
}
