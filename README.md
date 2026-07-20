# Sonora

Sonora is a beautiful, modern audio player written in C and GTK4. It is designed to be lightweight, fast, and feature-rich, providing a seamless music listening experience with built-in visualization and equalizer capabilities.

## Features

- **Local Playback**: Play your local audio files (`.flac`, `.mp3`, `.wav`, `.ogg`, `.m4a`).
- **Dynamic Library Scanner**: Automatically scans configured folders for new or deleted tracks in the background.
- **YouTube Streaming**: Stream and download audio directly from YouTube using integrated `yt-dlp` support.
- **10-Band Equalizer**: Built-in equalizer with a pre-amp and presets.
- **Visualizer**: Real-time audio visualization using PulseAudio.
- **Modern UI**: Dark-themed GTK4 interface with album cover extraction and caching.

## Dependencies

To compile Sonora, you need the following development libraries:

- `gtk4`
- `sqlite3`
- `libpulse-simple`
- `sndfile`
- `libcurl`

### Ubuntu / Debian
```bash
sudo apt install libgtk-4-dev libsqlite3-dev libpulse-dev libsndfile1-dev libcurl4-openssl-dev
```

### Fedora
```bash
sudo dnf install gtk4-devel sqlite-devel pulseaudio-libs-devel libsndfile-devel libcurl-devel
```

### Arch Linux
```bash
sudo pacman -S gtk4 sqlite libpulse libsndfile curl
```

## Building

Sonora uses a standard `Makefile`. Simply run:

```bash
make
```

This will produce the `sonora` executable in the project root.

## Building an AppImage

To build an AppImage that bundles the application and its dependencies (useful for distribution):

1. Make sure you have `linuxdeploy` and `linuxdeploy-plugin-appimage` downloaded.
2. Run the build script:

```bash
./build_appimage.sh
```

This will generate a `Sonora-x86_64.AppImage` file.

## Usage

You can run the player directly:

```bash
./sonora
```

Once running, navigate to the **Filesystem** tab to add folders containing your music. The background scanner will automatically add your tracks to the library.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
