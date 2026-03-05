#!/usr/bin/env python3
import sys
import json
import re
import os
from urllib.parse import urlparse, parse_qs
from playwright.sync_api import sync_playwright


def extract_video_id(url: str) -> str:
    """Extract video ID from various DouYin URL formats."""
    if not url.startswith("http"):
        url = "https://" + url

    parsed = urlparse(url)

    if "/video/" in url:
        parts = url.split("/video/")
        if len(parts) > 1:
            video_id = parts[1].split("?")[0].split("/")[0]
            if video_id.isdigit():
                return video_id

    if "/share/video/" in url:
        parts = url.split("/share/video/")
        if len(parts) > 1:
            video_id = parts[1].split("?")[0].split("/")[0]
            if video_id.isdigit():
                return video_id

    return None


def parse_douyin(url: str) -> list:
    """Parse DouYin URL and return download links for videos/images."""
    video_urls = []
    image_urls = []

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        if not url.startswith("http"):
            url = "https://" + url

        # Collect video URLs from network responses
        video_found_urls = set()

        def handle_response(response):
            resp_url = response.url
            # Look for actual video file URLs (not player JS)
            if (
                "douyinvod.com" in resp_url
                and "/video/" in resp_url
                and "?" in resp_url
            ):
                # Get the full URL
                if resp_url not in video_found_urls:
                    video_found_urls.add(resp_url)

        page.on("response", handle_response)

        # Navigate
        page.goto(url)

        # Wait for page to load and video to start playing
        page.wait_for_timeout(20000)

        # Add collected URLs - keep only one unique video
        # 抖音 CDN 多个 URL 指向相同内容，只保留一个最高画质的
        best_url = None
        best_br = 0

        for vurl in video_found_urls:
            parsed = urlparse(vurl)
            params = parse_qs(parsed.query)
            br = int(params.get("br", ["0"])[0])

            if br > best_br:
                best_br = br
                best_url = vurl

        if best_url:
            video_urls.append(best_url)

        # 去重 - 基于 video key（视频路径+比特率），只保留一个
        seen = set()
        final_urls = []
        for vurl in video_urls:
            if not vurl:
                continue
            parsed = urlparse(vurl)
            params = parse_qs(parsed.query)
            # 使用视频路径 + 比特率作为唯一key
            key = f"{parsed.path}_{params.get('br', ['0'])[0]}"
            if key not in seen:
                seen.add(key)
                final_urls.append(vurl)

        video_urls = final_urls[:1]  # 只保留一个最高画质的

        # For images - look for image posts
        page_images = page.evaluate("""() => {
            const result = [];
            
            // Get all images from the page - be more specific to avoid UI elements
            const imgs = document.querySelectorAll('img');
            for (let img of imgs) {
                const src = img.src || img.currentSrc;
                // Only keep content images (aweme_images, original posts)
                // Filter out: buttons, icons, avatars, UI elements
                if (src && (
                    src.includes('aweme_images') ||  // Original images in posts
                    (src.includes('douyinpic') && src.includes('~tplv') && !src.includes('button') && !src.includes('icon') && !src.includes('avatar') && !src.includes('Avatar'))
                )) {
                    result.push(src);
                }
            }
            
            return result;
        }""")

        for img in page_images:
            if img and img not in image_urls:
                image_urls.append(img)

        browser.close()

    # Clean URLs - separate video and image, filter properly
    cleaned = []

    # Video: keep only one highest quality
    video_urls_cleaned = []
    seen_videos = set()
    for vurl in video_urls:
        vurl = vurl.replace("&amp;", "&")
        vurl = re.sub(r"&#[0-9]+;", "", vurl)
        if not vurl or "douyinvod" not in vurl:
            continue
        parsed = urlparse(vurl)
        params = parse_qs(parsed.query)
        br = int(params.get("br", ["0"])[0])
        # Use bitrate as key, keep highest
        key = f"{parsed.path}_{br}"
        if key not in seen_videos:
            seen_videos.add(key)
            video_urls_cleaned.append((br, vurl))

    # Sort by bitrate and keep highest
    video_urls_cleaned.sort(key=lambda x: x[0], reverse=True)
    if video_urls_cleaned:
        cleaned.append(video_urls_cleaned[0][1])

    # Images: deduplicate and filter to keep only original quality
    image_urls_cleaned = []
    seen_images = set()
    for url in image_urls:
        url = url.replace("&amp;", "&")
        url = re.sub(r"&#[0-9]+;", "", url)
        if not url:
            continue
        parsed = urlparse(url)
        path = parsed.path

        # Skip thumbnails, cropped, private, UI elements, comments
        if any(
            x in url
            for x in [
                "-priv",
                "resize",
                "noop",
                "button",
                "icon",
                "avatar",
                "Avatar",
                "cover",
                "aweme_comment",  # Skip comment images
            ]
        ):
            continue

        # Only keep images from posts (should have specific path patterns)
        if "douyinpic" in url and "tos-cn-i-" not in path:
            continue

        # Get unique image identifier
        img_id = "/".join(path.split("/")[-2:]).split("~")[0]
        if img_id not in seen_images:
            seen_images.add(img_id)
            image_urls_cleaned.append(url)

    # Add unique images
    for url in image_urls_cleaned:
        if url not in cleaned:
            cleaned.append(url)

    return cleaned


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("[]")
        sys.exit(1)

    url = sys.argv[1]
    try:
        urls = parse_douyin(url)
        print(json.dumps(urls))
    except Exception as e:
        print(json.dumps({"error": str(e)}))
