#include <filesystem>
#include <string>
#include <tgbotxx/tgbotxx.hpp>
#include <thread>
#include <tuple>

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
      : tgbotxx::Bot(config.bot_token), download_dir_(std::move(config.download_dir)) {}

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
    const auto urls = Platform::GetUrls(message_content);
    if (urls.empty()) {
      return;
    }

    for (auto&& url : urls) {
      std::thread{[this, url = std::move(url), message, platform_name] {
        auto download_links = Platform::GetDownloadLinks(url);
        LOG_INFO("Processing URL {} in {}, get {} download_links", url, platform_name, download_links.size());
        for (auto&& download_link : download_links) {
          std::thread{[this, download_link = std::move(download_link), message] {
            LOG_INFO("Try downloading {}", download_link);
            if (const auto downloaded_filepath = Platform::DownloadFile(download_link, download_dir_)) {
              LOG_INFO("Uploading file {}", downloaded_filepath->string());
              cpr::File document(downloaded_filepath->string());
              cielparser::TryNTimes<3>([&] { api()->sendDocument(message->chat->id, document); });
            }
          }}.detach();
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
