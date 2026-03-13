#include <filesystem>
#include <future>
#include <optional>
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

 private:
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

    std::apply([&]<class... Platform>(Platform...) { (processMessage<Platform>(message, message_content), ...); },
               kPlatforms);
  }

  template <class Platform>
  void processMessage(const tgbotxx::Ptr<tgbotxx::Message>& message, const std::string_view message_content) const {
    constexpr auto platform_name = Platform::NAME;
    const bool is_group = message->chat->type != tgbotxx::Chat::Type::Private;

    for (const auto urls = Platform::GetUrls(message_content); auto&& url : urls) {
      std::thread{[=, this, url = std::move(url)] {
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

        if (downloaded_files.empty()) {
          cielparser::TryNTimes<3>([&] {
            api()->sendMessage(message->chat->id, std::format("Fail to download files from {}", url), 0, "", {}, false,
                               false, nullptr, "", 0, nullptr, false, "", nullptr,
                               cielparser::MakeReplyParameters(is_group, message->messageId));
          });
          return;
        }

        if (downloaded_files.size() == 1) {
          cielparser::TryNTimes<3>([&] {
            api()->sendDocument(message->chat->id, cpr::File(downloaded_files[0].string()), 0, std::monostate{},
                                std::format("[source]({})", url), "MarkdownV2", {}, false, false, nullptr, "", 0, false,
                                false, "", nullptr, cielparser::MakeReplyParameters(is_group, message->messageId));
          });
          return;
        }

        constexpr size_t chunk_size = 10;
        const size_t total_chunks = (downloaded_files.size() + 9) / 10;
        for (size_t i = 0; i < downloaded_files.size(); i += chunk_size) {
          std::vector<tgbotxx::Ptr<tgbotxx::InputMedia>> media_group;
          for (size_t j = i; j < std::min(i + chunk_size, downloaded_files.size()); ++j) {
            auto input_media = std::make_shared<tgbotxx::InputMediaDocument>();
            input_media->media = cpr::File(downloaded_files[j].string());
            media_group.emplace_back(std::move(input_media));
          }
          media_group.back()->caption =
              total_chunks > 1 ? std::format("[source]({}) \\[{}/{}\\]", url, i / chunk_size + 1, total_chunks)
                               : std::format("[source]({})", url);
          media_group.back()->parseMode = "MarkdownV2";

          cielparser::TryNTimes<3>([&] {
            api()->sendMediaGroup(message->chat->id, media_group, 0, false, false, "", 0, false, "",
                                  cielparser::MakeReplyParameters(is_group, message->messageId));
          });
        }
      }}.detach();
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
