"""共用工具：路径、歌名解析、归一化、QQ 音乐接口。"""
from __future__ import annotations

import os
import re
import unicodedata
from pathlib import Path

import requests

MUSIC_ROOT = Path(r"C:\Users\AA\Music")
PROJECT_DIR = MUSIC_ROOT / "MusicPlayer"
PLAYLIST_DIR = PROJECT_DIR / "playlists"
LYRIC_DIR = PROJECT_DIR / "Lyrics"
BILI_DIR = PROJECT_DIR / "B站下载"
ORGANIZED_DIR = PROJECT_DIR / "整理后"

EXCLUDE_DIRS = {"MusicPlayer", "整理后", "B站下载"}
AUDIO_EXTS = {".m4a", ".mp3", ".flac", ".wav", ".aac", ".ogg", ".wma"}

QQ_HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/125.0 Safari/537.36",
    "Referer": "https://y.qq.com/",
}

def playlist_folders(root: Path = MUSIC_ROOT) -> list[Path]:
    return sorted(
        p for p in root.iterdir()
        if p.is_dir() and p.name not in EXCLUDE_DIRS and not p.name.startswith(".")
    )

def audio_files(folder: Path) -> list[Path]:
    return sorted(
        (p for p in folder.iterdir() if p.is_file() and p.suffix.lower() in AUDIO_EXTS),
        key=lambda p: natural_key(p.name),
    )

def natural_key(s: str):
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", s)]

def parse_song_name(filename: str) -> tuple[str, str]:
    """从文件名解析 (歌名, 歌手)，兼容 001.歌-歌手 / 001. 歌 - 歌手 / 歌 - 歌手 等。"""
    s = Path(filename).stem
    s = re.sub(r"^\d{1,4}\s*[.．_、\-）)]\s*", "", s)
    s = re.sub(r"\s*\(\d+\)\s*$", "", s)
    s = re.sub(r"\.{2,}\d+\s*$", "", s)
    s = re.sub(r"【[^】]*】\s*$", "", s)
    s = s.strip()

    parts = re.split(r"\s+[-–—]\s+|\s+[-–—]|[-–—]\s+", s, maxsplit=1)
    if len(parts) == 2 and parts[0].strip() and parts[1].strip():
        title, artist = parts[0].strip(), parts[1].strip()
    else:
        title, artist = s, ""
    return title, artist

def normalize_key(title: str, artist: str = "") -> str:
    """去除版本/空格/标点差异，用于同文件夹内判重。"""
    raw = f"{title} {artist}".strip()
    raw = unicodedata.normalize("NFKC", raw).lower()
    raw = re.sub(r"[\(（].*?[\)）]", "", raw)
    raw = re.sub(r"\[.*?\]|【.*?】", "", raw)
    raw = re.sub(r"[\s\-_·.,'\"!?~～！？，。、：:；;]", "", raw)
    raw = re.sub(r"(live|cover|remix|伴奏|现场|新版|版本|加速版|女生版|男生版|beat版|纯音乐|instrumental)", "", raw)
    return raw

def sanitize_filename(s: str) -> str:
    s = re.sub(r'[\\/:*?"<>|]', "_", s)
    return s.strip(" .")[:120] or "未命名"

def qq_search(keyword: str, n: int = 5) -> list[dict]:
    url = "https://c.y.qq.com/soso/fcgi-bin/client_search_cp"
    r = requests.get(
        url,
        params={"p": 1, "n": n, "w": keyword, "format": "json", "new_json": 1},
        headers=QQ_HEADERS,
        timeout=20,
    )
    r.raise_for_status()
    return r.json().get("data", {}).get("song", {}).get("list", [])

def qq_lyric(title: str, artist: str = "") -> str:
    songs = qq_search(f"{title} {artist}".strip(), n=3)
    if not songs:
        return ""
    mid = songs[0].get("mid", "")
    if not mid:
        return ""
    r = requests.get(
        "https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg",
        params={
            "songmid": mid,
            "g_tk": "5381",
            "format": "json",
            "inCharset": "utf-8",
            "outCharset": "utf-8",
            "nobase64": "1",
        },
        headers=QQ_HEADERS,
        timeout=20,
    )
    r.raise_for_status()
    return r.json().get("lyric", "") or ""

def read_playlist(path: Path) -> list[tuple[str, str]]:
    songs = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        title, artist = parse_song_name(line)
        if title:
            songs.append((title, artist))
    return songs
