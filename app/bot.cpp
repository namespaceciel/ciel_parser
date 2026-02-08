#include <filesystem>
#include <string>
#include <tgbotxx/tgbotxx.hpp>
#include <thread>

#include "config.hpp"
#include "pixiv.hpp"
#include "quill.hpp"
#include "twitter.hpp"
#include "utils.hpp"
#include "weibo.hpp"
#include "xhs.hpp"

class Bot final : public tgbotxx::Bot {
 public:
  explicit Bot(cielparser::Config config) : tgbotxx::Bot(config.bot_token), config_(std::move(config)) {}

  ~Bot() override = default;

 private:
  void onAnyMessage(const tgbotxx::Ptr<tgbotxx::Message>& message) override {
    const std::string_view message_content = !message->text.empty() ? message->text : message->caption;
    const std::string_view user_name =
        message->chat->type == tgbotxx::Chat::Type::Private ? message->from->username : message->chat->username;
    LOG_INFO("Received message {} from @{}", message_content, user_name);

    OnCustomPlatformMessage<cielparser::XHS>(message, message_content);
    OnCustomPlatformMessage<cielparser::WeiBo>(message, message_content);
    OnCustomPlatformMessage<cielparser::Twitter>(message, message_content);
    OnCustomPlatformMessage<cielparser::Pixiv>(message, message_content);
  }

  template <class Platform>
  void OnCustomPlatformMessage(const tgbotxx::Ptr<tgbotxx::Message>& message,
                               const std::string_view message_content) const {
    constexpr std::string_view PlatformName = []() {
      if constexpr (std::is_same_v<Platform, cielparser::XHS>) {
        return "XHS";
      } else if constexpr (std::is_same_v<Platform, cielparser::WeiBo>) {
        return "WeiBo";
      } else if constexpr (std::is_same_v<Platform, cielparser::Twitter>) {
        return "Twitter";
      } else if constexpr (std::is_same_v<Platform, cielparser::Pixiv>) {
        return "Pixiv";
      } else {
        static_assert(false);
      }
    }();

    for (const auto urls = Platform::GetUrls(message_content); const auto& url : urls) {
      std::jthread{[=, this] {
        const auto download_links = Platform::GetDownloadLinks(url);
        LOG_INFO("Processing URL {} in {}, get {} download_links", url, PlatformName, download_links.size());
        for (const auto& download_link : download_links) {
          std::jthread{[=, this] {
            LOG_INFO("Try downloading {}", download_link);
            if (const auto downloaded_filepath = Platform::DownloadFile(download_link, config_.download_dir)) {
              LOG_INFO("Uploading file {}", downloaded_filepath->string());
              cpr::File document(downloaded_filepath->string());
              cielparser::TryNTimes<3>([&]() { api()->sendDocument(message->chat->id, document); });
            }
          }}.detach();
        }
      }}.detach();
    }
  }

  cielparser::Config config_;
};

int main() {
  const char* config_path = std::getenv("CIELPARSER_CONFIG_PATH");
  if (!config_path) {
    LOG_ERROR("CIELPARSER_CONFIG_PATH env not set");
    return 1;
  }
  if (!std::filesystem::exists(config_path)) {
    LOG_ERROR("Config file {} not found", config_path);
    return 1;
  }

  cielparser::Config config = nlohmann::json::parse(std::ifstream{config_path});
  std::filesystem::create_directories(config.download_dir);
  Bot bot(std::move(config));
  bot.start();
}
