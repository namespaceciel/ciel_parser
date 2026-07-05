# ciel_parser

A C++23 Telegram bot that automatically detects social media URLs in messages, extracts download links for images and videos, downloads them locally, and sends the media back to the chat.

## Supported Platforms

| Platform | URL Patterns |
|----------|-------------|
| XiaoHongShu (RED) | `xiaohongshu.com`, `xhslink.com` |
| Weibo | `weibo.com`, `weibo.cn` |
| Twitter / X | `twitter.com/*/status/*`, `x.com/*/status/*` |
| Pixiv | `pixiv.net/artworks/<id>` |
| Bilibili | `bilibili.com/video/*`, `bilibili.com/opus/*`, `b23.tv`, `bili2233.cn` |
| Douyin | `v.douyin.com/<slug>` |

## Dependencies

| Dependency | Purpose |
|-----------|---------|
| [`telegram-bot-api`](https://github.com/tdlib/telegram-bot-api) | Local Telegram Bot API server (must be in `$PATH`) |
| Playwright | Headless browser for Douyin (`pip install playwright && playwright install chromium`) |

### Auto-fetched Libraries (via CMake FetchContent)

- [nlohmann/json](https://github.com/nlohmann/json) v3.12.0 — JSON parsing
- [quill](https://github.com/odygrd/quill) v12.0.0 — Async logging
- [tgbotxx](https://github.com/vir-bjoern/tgbotxx) v1.2.9.5 — Telegram Bot API wrapper
- [Boost](https://www.boost.org/) 1.90.0 — Process management
- [gflags](https://github.com/gflags/gflags) v2.3.0 — CLI flag parsing

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build --target ciel_parser_bot
```

The output binary is `build/ciel_parser_bot`.

## Configuration

Create a `config.json` file (see `config.json.example` for a template):

```json
{
    "bot_token": "YOUR_BOT_TOKEN_HERE",
    "api_id": "YOUR_API_ID",
    "api_hash": "YOUR_API_HASH",
    "tg_api_http_port": "8081",
    "download_dir": "/path/to/downloads",
    "log_path": "/path/to/logs/output.log"
}
```

| Field | Description |
|-------|-------------|
| `bot_token` | Telegram Bot API token (from [@BotFather](https://t.me/BotFather)) |
| `api_id` | Telegram API ID (for local bot API server, obtain from [my.telegram.org](https://my.telegram.org)) |
| `api_hash` | Telegram API hash (for local bot API server, obtain from [my.telegram.org](https://my.telegram.org)) |
| `tg_api_http_port` | HTTP port for the local `telegram-bot-api` server |
| `download_dir` | Directory where downloaded media files are saved |
| `log_path` | Path to log file (rotating, 1 MB per file) |

## Run

```bash
./build/ciel_parser_bot --config ./config.json
```
