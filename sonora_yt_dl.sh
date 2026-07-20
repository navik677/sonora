#!/bin/bash
# sonora_yt_dl.sh
# Downloads a YouTube video and converts to FLAC using yt-dlp

URL="$1"
MUSIC_DIR="$2"

if [ -z "$URL" ] || [ -z "$MUSIC_DIR" ]; then
    echo "Usage: $0 <URL> <MUSIC_DIR>"
    exit 1
fi

mkdir -p "$MUSIC_DIR"

# Resolve yt-dlp path relative to this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
YT_DLP="$SCRIPT_DIR/yt-dlp"

if [ ! -x "$YT_DLP" ]; then
    YT_DLP="yt-dlp"
fi

echo "[0.00] Starting YouTube FLAC download..."

"$YT_DLP" \
  -x --audio-format flac \
  --embed-metadata --embed-thumbnail \
  -o "$MUSIC_DIR/%(title)s.%(ext)s" \
  --newline \
  "$URL" 2>&1 | awk '
BEGIN {
    fflush()
}
/^\[download\][ \t]+([0-9]+\.[0-9]+)%/ {
    match($0, /([0-9]+\.[0-9]+)%/, arr)
    pct = arr[1] / 100.0
    printf "[%.2f] Downloading... %.1f%%\n", pct, arr[1]
    fflush()
}
/^\[ExtractAudio\]/ {
    print "[1.00] Converting to FLAC..."
    fflush()
}
/^\[download\] Destination/ {
    match($0, /Destination: (.*)/, arr)
    printf "[0.00] Downloading %s...\n", arr[1]
    fflush()
}
END {
    print "[1.00] Download Complete."
    fflush()
}
'
