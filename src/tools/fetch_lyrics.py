"""批量为本地歌曲获取并缓存歌词（QQ 音乐接口）。已存在 .lrc 的会跳过。"""
from __future__ import annotations

import argparse
import time
from pathlib import Path

from common import (
    LYRIC_DIR, MUSIC_ROOT, audio_files, parse_song_name,
    playlist_folders, qq_lyric, sanitize_filename,
)

def lrc_path(title: str, artist: str) -> Path:
    name = sanitize_filename(title) + (f"_{sanitize_filename(artist)}" if artist else "")
    return LYRIC_DIR / f"{name}.lrc"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=0, help="最多处理多少首（0=全部）")
    ap.add_argument("--force", action="store_true", help="覆盖已有歌词")
    ap.add_argument("--delay", type=float, default=0.12, help="每首之间延时（秒）")
    args = ap.parse_args()

    LYRIC_DIR.mkdir(parents=True, exist_ok=True)
    songs = []
    for folder in playlist_folders():
        for file in audio_files(folder):
            title, artist = parse_song_name(file.name)
            songs.append((file, title, artist))

    done = skipped = failed = 0
    for i, (file, title, artist) in enumerate(songs):
        if args.limit and done + skipped >= args.limit:
            break
        near = file.with_suffix(".lrc")
        cached = lrc_path(title, artist)
        if not args.force and (near.exists() or cached.exists()):
            skipped += 1
            continue
        try:
            lrc = qq_lyric(title, artist)
            if not lrc:
                failed += 1
                print(f"[无歌词] {title} - {artist}")
            else:
                cached.write_text(lrc, encoding="utf-8")
                done += 1
                print(f"[获取] {title} - {artist} ({len(lrc)} 字符)")
        except Exception as e:
            failed += 1
            print(f"[失败] {title} - {artist}: {e}")
        time.sleep(args.delay)

    print(f"\n完成：新增 {done}，跳过 {skipped}，失败 {failed}；共 {len(songs)} 首")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
