"""根据歌单 txt 到 B 站搜索并下载音频到单独文件夹（直接走 B 站 DASH 音频流，无需 ffmpeg）。

示例：
  python bili_audio.py "playlists/老歌.txt" --limit 5
  python bili_audio.py "playlists/老歌.txt" --dry-run

注意：请仅下载你有权保存的音频，尊重版权；本工具只做个人曲库整理。
"""
from __future__ import annotations

import argparse
import html
import re
import sys
import time
from pathlib import Path

import requests

from common import BILI_DIR, PLAYLIST_DIR, normalize_key, read_playlist, sanitize_filename

UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/125.0 Safari/537.36"

def make_session() -> requests.Session:
    s = requests.Session()
    s.headers.update({"User-Agent": UA, "Referer": "https://www.bilibili.com/"})
    s.get("https://www.bilibili.com/", timeout=20)
    return s

def refresh_session(session: requests.Session):
    fresh = make_session()
    session.cookies.clear()
    session.cookies.update(fresh.cookies)

def strip_tags(s: str) -> str:
    return html.unescape(re.sub(r"<[^>]+>", "", s or "")).strip()

def to_float(v) -> float:
    try:
        return float(v)
    except (TypeError, ValueError):
        return 0.0

def score_result(title: str, artist: str, item: dict) -> float:
    t = strip_tags(item.get("title", "")).lower()
    author = (item.get("author") or "").lower()
    key = normalize_key(title)
    text = normalize_key(t)
    sc = 0.0
    if key and key in text:
        sc += 12
    elif key:
        sc += sum(4 for w in normalize_key(t).split() if len(w) > 2 and w in key)
    if artist and normalize_key(artist) in normalize_key(t + author):
        sc += 7
    if any(w in t for w in ("翻唱", "cover", "鬼畜", "ai", "remix", "变速", "钢琴", "吉他", "舞蹈")):
        sc -= 6
    if any(w in t for w in ("hi-res", "无损", "hi fi", "hifi", "flac")):
        sc += 3
    sc += min(to_float(item.get("play")), 2_000_000) / 200_000
    sc -= min(to_float(item.get("duration")), 10) / 20
    return sc

def search(session: requests.Session, title: str, artist: str, limit: int = 15) -> dict | None:
    keyword = f"{title} {artist}".strip()
    for attempt in range(3):
        try:
            r = session.get(
                "https://api.bilibili.com/x/web-interface/search/all/v2",
                params={"keyword": keyword},
                timeout=20,
            )
            if r.status_code != 200:
                refresh_session(session)
                time.sleep(1.5 * (attempt + 1))
                continue
            sections = (r.json().get("data") or {}).get("result") or []
            results = []
            for sec in sections:
                if sec.get("result_type") == "video":
                    results = sec.get("data") or []
                    break
            if not results:
                return None
            return max(results[:limit], key=lambda x: score_result(title, artist, x))
        except Exception:
            time.sleep(1.5 * (attempt + 1))
    return None

def get_audio_stream(session: requests.Session, bvid: str) -> list[str] | None:
    """返回按码率排序的音频候选 URL（baseUrl + backupUrl）。"""
    try:
        v = session.get(
            "https://api.bilibili.com/x/web-interface/view", params={"bvid": bvid}, timeout=20
        ).json()
        cid = (v.get("data") or {}).get("cid")
        if not cid:
            return None
        p = session.get(
            "https://api.bilibili.com/x/player/playurl",
            params={"bvid": bvid, "cid": cid, "qn": 64, "fnval": 16, "fnver": 0, "fourk": 0},
            headers={"Referer": f"https://www.bilibili.com/video/{bvid}/"},
            timeout=20,
        ).json()
        audios = ((p.get("data") or {}).get("dash") or {}).get("audio") or []
        if not audios:
            return None
        audios.sort(key=lambda a: to_float(a.get("bandwidth")), reverse=True)
        urls = []
        for a in audios[:3]:
            if a.get("baseUrl"):
                urls.append(a["baseUrl"])
            for u in (a.get("backupUrl") or [])[:2]:
                urls.append(u)
        return urls
    except Exception:
        return None

def download_one(session: requests.Session, bvid: str, out_base: Path) -> bool:
    urls = get_audio_stream(session, bvid)
    if not urls:
        print("    未取得音频流")
        return False
    headers = {"Referer": f"https://www.bilibili.com/video/{bvid}/", "Origin": "https://www.bilibili.com"}
    final = Path(str(out_base) + ".m4a")
    tmp = Path(str(out_base) + ".part")
    for url in urls:
        try:
            with session.get(url, headers=headers, stream=True, timeout=(10, 40)) as r:
                if r.status_code != 200:
                    continue
                total = 0
                with open(tmp, "wb") as f:
                    for chunk in r.iter_content(262144):
                        if chunk:
                            f.write(chunk)
                            total += len(chunk)
            if total > 100_000:
                tmp.replace(final)
                return True
        except Exception:
            continue
        finally:
            if tmp.exists():
                tmp.unlink(missing_ok=True)
    return False

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("playlist", help="playlists 目录下的歌单 txt（或完整路径）")
    ap.add_argument("--limit", type=int, default=0, help="最多处理多少首（0=全部）")
    ap.add_argument("--dry-run", action="store_true", help="只搜索并列出结果，不下载")
    ap.add_argument("--delay", type=float, default=0.8, help="每首之间的延时（秒）")
    args = ap.parse_args()

    pl = Path(args.playlist)
    if not pl.exists():
        pl = PLAYLIST_DIR / pl.name
    if not pl.exists():
        print(f"找不到歌单: {args.playlist}", file=sys.stderr)
        return 1

    songs = read_playlist(pl)
    outdir = BILI_DIR / pl.stem
    outdir.mkdir(parents=True, exist_ok=True)
    session = make_session()

    ok = miss = skip = handled = 0
    for i, (title, artist) in enumerate(songs, 1):
        if args.limit and handled >= args.limit:
            break
        handled += 1
        prefix = f"{i:03d}."
        if list(outdir.glob(f"{prefix} *")) and not args.dry_run:
            skip += 1
            continue
        item = search(session, title, artist)
        if not item:
            miss += 1
            print(f"[{i:03d}] 未找到: {title} - {artist}")
            continue
        print(f"[{i:03d}] {title} - {artist}  ->  {strip_tags(item.get('title',''))[:52]} ({item.get('bvid')})")
        if args.dry_run:
            continue
        out_base = outdir / f"{prefix} {sanitize_filename(title)} - {sanitize_filename(artist)}"
        if download_one(session, item["bvid"], out_base):
            ok += 1
        else:
            miss += 1
        time.sleep(args.delay)

    print(f"\n完成：下载 {ok}，跳过 {skip}，失败 {miss}；输出目录 {outdir}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
