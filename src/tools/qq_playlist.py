"""提取 QQ 音乐歌单到本地 txt。

用法:
  python qq_playlist.py <歌单链接或ID> [--name 歌单名]
  python qq_playlist.py --local            # 从 C:\\Users\\AA\\Music 现有文件夹生成歌单
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import requests

from common import (
    MUSIC_ROOT, PLAYLIST_DIR, QQ_HEADERS, audio_files,
    parse_song_name, playlist_folders, sanitize_filename,
)

def extract_id(text: str) -> str:
    if text.isdigit():
        return text
    for pattern in (r"/playlist/(\d+)", r"[?&]id=(\d+)", r"dissid[/=](\d+)"):
        m = re.search(pattern, text)
        if m:
            return m.group(1)
    raise ValueError(f"无法从输入中识别歌单 ID: {text}")

def fetch_online(playlist_id: str) -> tuple[str, list[tuple[str, str]]]:
    r = requests.get(
        "https://c.y.qq.com/v8/fcg-bin/fcg_v8_playlist_cp.fcg",
        params={"format": "json", "id": playlist_id},
        headers=QQ_HEADERS,
        timeout=30,
    )
    r.raise_for_status()
    data = r.json()
    cdlist = data.get("data", {}).get("cdlist", [])
    if not cdlist:
        raise RuntimeError("歌单不存在或接口返回异常")
    info = cdlist[0]
    name = info.get("dissname") or f"QQ歌单_{playlist_id}"
    songs = []
    for s in info.get("songlist", []):
        title = s.get("name") or s.get("title") or ""
        singers = s.get("singer") or []
        artist = "、".join(x.get("name", "") for x in singers if x.get("name"))
        if title:
            songs.append((title, artist))
    return sanitize_filename(name), songs

def write_playlist(name: str, songs: list[tuple[str, str]]) -> Path:
    PLAYLIST_DIR.mkdir(parents=True, exist_ok=True)
    out = PLAYLIST_DIR / f"{sanitize_filename(name)}.txt"
    lines = [f"# QQ音乐歌单: {name}  共 {len(songs)} 首"] + [
        f"{title} - {artist}" if artist else title for title, artist in songs
    ]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out

def export_local() -> list[Path]:
    outputs = []
    for folder in playlist_folders():
        songs = [parse_song_name(p.name) for p in audio_files(folder)]
        out = write_playlist(folder.name, songs)
        outputs.append(out)
        print(f"[本地] {folder.name}: {len(songs)} 首 -> {out}")
    return outputs

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("playlist", nargs="?", help="QQ 音乐歌单链接或 ID")
    ap.add_argument("--name", help="输出歌单名")
    ap.add_argument("--local", action="store_true", help="从本地文件夹生成歌单")
    args = ap.parse_args()

    if args.local or not args.playlist:
        export_local()
        return 0
    try:
        pid = extract_id(args.playlist)
        name, songs = fetch_online(pid)
        if args.name:
            name = args.name
        out = write_playlist(name, songs)
        print(f"[在线] {name}: {len(songs)} 首 -> {out}")
        return 0
    except Exception as e:
        print(f"[失败] {e}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
