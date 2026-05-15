# LoraRadio

A desktop media player for Windows with support for internet radio and YouTube audio.  
Built with C++20 and Qt 6.

![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Qt](https://img.shields.io/badge/Qt-6.9-green)
![C++](https://img.shields.io/badge/C++-20-orange)

---

## Features

- Internet radio playback (HTTP/HTTPS streams)
- YouTube audio playback without a browser
- Station management: add, remove, edit
- Per-station volume memory
- System tray with quick controls
- Windows autostart support
- Dark theme
- English and Russian language support

---

## Screenshots

> *Add screenshots here*

---

## Requirements

### Build dependencies

| Tool | Version |
|---|---|
| Qt | 6.9+ (MinGW 64-bit) |
| CMake | 3.31+ |
| MinGW | 64-bit (bundled with Qt) |

### Third-party executables

Place the following files into the `thirdparty/` folder before building:

| File | Download |
|---|---|
| `ffmpeg.exe` | https://ffmpeg.org/download.html |
| `ffplay.exe` | https://ffmpeg.org/download.html (bundled with ffmpeg) |
| `yt-dlp.exe` | https://github.com/yt-dlp/yt-dlp/releases |

---

## Building

### 1. Clone the repository

```bash
git clone https://github.com/your-username/LoraRadio.git
cd LoraRadio
```

### 2. Place third-party files

```
LoraRadio/
└── thirdparty/
    ├── ffmpeg.exe
    ├── ffplay.exe
    └── yt-dlp.exe
```

### 3. Open in Qt Creator

- Open `CMakeLists.txt` via **File → Open File or Project**
- Select the **Desktop Qt 6.9.0 MinGW 64-bit** kit
- Click **Build**

### 4. Or build from the command line

```bash
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.9.0/mingw_64"
cmake --build build --config Release
```

> Replace the Qt path with your own installation path.

---

## Project Structure

```
LoraRadio/
├── src/
│   ├── main.cpp              # Entry point, initialization
│   ├── MainWindow.cpp/h      # Main application window
│   ├── RadioPlayer.cpp/h     # Internet radio playback (QMediaPlayer)
│   ├── YTPlayer.cpp/h        # YouTube playback (yt-dlp + ffmpeg)
│   ├── SwitchPlayer.cpp/h    # Proxy: routes play() to the correct player
│   ├── StationManager.cpp/h  # Station list storage and management
│   ├── RadioPage.cpp/h       # Radio tab UI
│   ├── YouTubePage.cpp/h     # YouTube tab UI
│   └── ...
├── include/
│   ├── AbstractPlayer.h      # Abstract player interface
│   └── ...
├── thirdparty/               # ffmpeg.exe, yt-dlp.exe (not tracked by git)
└── CMakeLists.txt
```

---

## How YouTube Playback Works

No YouTube API is used. Instead:

1. `yt-dlp` is launched as a child process and extracts a direct audio stream URL
2. `ffmpeg` decodes the stream into raw PCM (48000 Hz, Stereo, Int16)
3. PCM data is fed into `QAudioSink` and played through the system audio output

If the connection drops, `YTPlayer` automatically retries up to 5 times with exponential backoff.

---

## Data & Settings

All data is stored locally:

| What | Where |
|---|---|
| Station list | `%AppData%/LoraRadio/stations.json` |
| Settings (language, volume) | Windows Registry / INI file via `QSettings` |
| Logs | `logs/` folder next to the executable |

---

## Dependencies

- [Qt 6](https://www.qt.io/) — GUI and multimedia (LGPLv3)
- [yt-dlp](https://github.com/yt-dlp/yt-dlp) — YouTube stream extraction (Unlicense)
- [FFmpeg](https://ffmpeg.org/) — audio decoding (LGPL 2.1+)

---

## License

MIT License. See [LICENSE](LICENSE) for details.
