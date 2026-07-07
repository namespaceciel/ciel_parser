# ciel_parser

A C++23 Telegram bot that detects social media URLs in messages, extracts media download links, and sends the content back to the chat.

## Supported Platforms

| Platform | URL Patterns |
|----------|-------------|
| XiaoHongShu | `xiaohongshu.com`, `xhslink.com` |
| Douyin | `v.douyin.com`, `douyin.com/video/`, `douyin.com/note/`, `iesdouyin.com/share/` |
| Weibo | `weibo.com`, `weibo.cn` |
| Bilibili | `bilibili.com/video/`, `bilibili.com/opus/`, `b23.tv`, `bili2233.cn` |
| Twitter / X | `twitter.com/*/status/`, `x.com/*/status/` |
| Pixiv | `pixiv.net/artworks/<id>` |

## Dependencies

- [telegram-bot-api](https://github.com/tdlib/telegram-bot-api) — must be in `$PATH`

**Auto-fetched by CMake:**

- [nlohmann/json](https://github.com/nlohmann/json) v3.12.0
- [quill](https://github.com/odygrd/quill) v12.0.0
- [tgbotxx](https://github.com/vir-bjoern/tgbotxx) v1.2.9.5
- [Boost](https://www.boost.org/) 1.90.0
- [gflags](https://github.com/gflags/gflags) v2.3.0

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build --target ciel_parser_bot
```

Binary: `build/ciel_parser_bot`.

## Configuration

Create `config.json` (see `config.json.example`):

```json
{
    "bot_token": "YOUR_BOT_TOKEN",
    "api_id": "YOUR_API_ID",
    "api_hash": "YOUR_API_HASH",
    "tg_api_http_port": "8081",
    "download_dir": "/path/to/downloads",
    "log_path": "/path/to/logs/output.log"
}
```

| Field | Description |
|-------|-------------|
| `bot_token` | From [@BotFather](https://t.me/BotFather) |
| `api_id` / `api_hash` | From [my.telegram.org](https://my.telegram.org) |
| `tg_api_http_port` | HTTP port for `telegram-bot-api` |
| `download_dir` | Downloaded media storage |
| `log_path` | Rotating log file |

## Run

```bash
./build/ciel_parser_bot --config ./config.json
```
