"""整理曲库：同文件夹内去重、统一命名为「001. 歌名 - 歌手.ext」，跨文件夹相同歌曲保留。

默认行为（不加参数）：
  1) 扫描并生成报告 MusicPlayer/整理报告.txt
  2) 在 MusicPlayer/整理后/<原文件夹>/ 创建 NTFS 硬链接视图，不改动也不删除任何原文件

如需真正改写原文件，请显式使用 --apply-inplace --yes（会先备份），一般不需要。
"""
from __future__ import annotations

import argparse
import os
import shutil
from collections import defaultdict
from pathlib import Path

from common import (
    MUSIC_ROOT, ORGANIZED_DIR, PROJECT_DIR, audio_files,
    normalize_key, parse_song_name, playlist_folders, sanitize_filename,
)

def link_or_copy(src: Path, dst: Path):
    try:
        os.link(src, dst)
        return "硬链接"
    except OSError:
        shutil.copy2(src, dst)
        return "复制"

def scan(root: Path = MUSIC_ROOT):
    report = []
    total = dup_count = 0
    organized_files = 0
    for folder in playlist_folders(root):
        files = audio_files(folder)
        groups: dict[str, list[Path]] = defaultdict(list)
        for file in files:
            title, artist = parse_song_name(file.name)
            groups[normalize_key(title, artist)].append(file)

        keepers, duplicates = [], []
        for key, group in groups.items():
            group.sort(key=lambda p: p.name)
            keepers.append(group[0])
            duplicates.extend(group[1:])

        keepers.sort(key=lambda p: p.name)
        total += len(files)
        dup_count += len(duplicates)

        report.append(f"== {folder.name} ==  共 {len(files)} 首，保留 {len(keepers)}，重复 {len(duplicates)}")
        for new_index, file in enumerate(keepers, 1):
            title, artist = parse_song_name(file.name)
            new_name = f"{new_index:03d}. {sanitize_filename(title)}" + (f" - {sanitize_filename(artist)}" if artist else "") + file.suffix.lower()
            report.append(f"  {new_index:03d}  {file.name}  ->  {new_name}")
        for file in duplicates:
            report.append(f"  [重复] {file.name}")

        outfolder = ORGANIZED_DIR / folder.name
        outfolder.mkdir(parents=True, exist_ok=True)
        m3u = []
        for new_index, file in enumerate(keepers, 1):
            title, artist = parse_song_name(file.name)
            new_name = f"{new_index:03d}. {sanitize_filename(title)}" + (f" - {sanitize_filename(artist)}" if artist else "") + file.suffix.lower()
            dst = outfolder / new_name
            if not dst.exists():
                link_or_copy(file, dst)
                organized_files += 1
            m3u.append(new_name)
        (outfolder / "playlist.m3u").write_text("\n".join(m3u) + "\n", encoding="utf-8")
        report.append("")

    summary = f"扫描 {total} 首；同文件夹重复 {dup_count} 首；整理视图写入 {organized_files} 个链接；跨文件夹相同歌曲均保留。"
    report.insert(0, summary + "\n")
    (PROJECT_DIR / "整理报告.txt").write_text("\n".join(report), encoding="utf-8")
    print(summary)
    print(f"报告: {PROJECT_DIR / '整理报告.txt'}")
    print(f"整理视图: {ORGANIZED_DIR}")
    return total, dup_count

def apply_inplace():
    """高风险操作：在用户显式确认后才调用。先备份，再重命名/移走重复项。"""
    backup = PROJECT_DIR / "原文件备份"
    if backup.exists():
        raise RuntimeError("备份目录已存在，请先确认其内容，避免覆盖")
    backup.mkdir(parents=True)
    for folder in playlist_folders():
        files = audio_files(folder)
        groups: dict[str, list[Path]] = defaultdict(list)
        for file in files:
            title, artist = parse_song_name(file.name)
            groups[normalize_key(title, artist)].append(file)
        for key, group in groups.items():
            group.sort(key=lambda p: p.name)
            for file in group[1:]:
                shutil.move(str(file), str(backup / f"{folder.name}__{file.name}"))
        keepers = sorted([g[0] for g in groups.values()], key=lambda p: p.name)
        for i, file in enumerate(keepers, 1):
            title, artist = parse_song_name(file.name)
            new_name = f"{i:03d}. {sanitize_filename(title)}" + (f" - {sanitize_filename(artist)}" if artist else "") + file.suffix.lower()
            if file.name != new_name:
                file.rename(folder / new_name)
    print(f"已原地整理，原文件备份在 {backup}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply-inplace", action="store_true", help="危险：原地重命名并移走重复项")
    ap.add_argument("--yes", action="store_true", help="必须同时指定才允许原地整理")
    args = ap.parse_args()
    if args.apply_inplace:
        if not args.yes:
            print("原地整理会移动个人文件，请先备份并显式加 --yes 确认。默认模式只生成安全整理视图。")
            return 2
        apply_inplace()
        return 0
    scan()
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
