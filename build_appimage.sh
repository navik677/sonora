#!/bin/bash
set -e

APP_NAME="Sonora"
BIN="sonora"

echo "Building Sonora..."
make clean && make

echo "Setting up AppDir..."
rm -rf AppDir
mkdir -p AppDir/usr/bin AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/scalable/apps

cp $BIN AppDir/usr/bin/
cp sonora.desktop AppDir/usr/share/applications/
cp sonora.svg AppDir/usr/share/icons/hicolor/scalable/apps/
cp sonora.svg AppDir/sonora.svg
cp sonora.desktop AppDir/sonora.desktop

# Download and bundle yt-dlp for YT Music streaming
if [ ! -f yt-dlp_linux ]; then
    echo "Downloading yt-dlp..."
    wget -q https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_linux
    chmod +x yt-dlp_linux
fi
cp yt-dlp_linux AppDir/usr/bin/yt-dlp


# Download and bundle static jq for JSON parsing
if [ ! -f jq_static ]; then
    echo "Downloading jq static..."
    wget -q https://github.com/jqlang/jq/releases/latest/download/jq-linux-amd64 -O jq_static
    chmod +x jq_static
fi
cp jq_static AppDir/usr/bin/jq

cat << 'EOF' > AppDir/usr/bin/sonora_khinsider.sh
#!/bin/bash
url="$1"
dir="$2"
mkdir -p "$dir"
echo "PROG:0.0:Fetching album metadata..."
album_id=$(basename "$url")
links=$(curl -s "$url" | grep -o "href=\"/game-soundtracks/album/$album_id/[^\"]*\"" | cut -d'"' -f2 | grep -v 'change_log' | sort -u)
total=$(echo "$links" | wc -w)
if [ "$total" -eq 0 ]; then
  echo "PROG:1.0:Error: No tracks found"
  exit 1
fi
cur=0
for path in $links; do
  cur=$((cur+1))
  pct=$(awk "BEGIN {print $cur / $total}")
  echo "PROG:$pct:Parsing track $cur/$total..."
  flac_link=$(curl -s "https://downloads.khinsider.com$path" | grep -io 'href="[^"]*\.flac"' | head -n 1 | cut -d'"' -f2)
  if [ ! -z "$flac_link" ]; then
    name=$(basename "$flac_link" | sed 's/%20/ /g')
    echo "PROG:$pct:Downloading $name..."
    curl -L "$flac_link" -o "$dir/$name" -s
  fi
done
echo "PROG:1.0:Download complete!"
EOF
cp sonora_yt_dl.sh AppDir/usr/bin/
chmod +x AppDir/usr/bin/sonora_khinsider.sh AppDir/usr/bin/sonora_yt_dl.sh

cat << 'EOF' > AppDir/usr/bin/sonora_archive.sh
#!/bin/bash
url="$1"
dir="$2"
mkdir -p "$dir"
id=$(basename "$url")
echo "PROG:0.0:Fetching Archive.org metadata..."

files_json=$(curl -s "https://archive.org/metadata/$id")

# Try to find FLAC first
files=$(echo "$files_json" | jq -r '.files[] | select(.format | test("flac"; "i")) | .name')

# If no FLAC, try MP3
if [ -z "$files" ]; then
    files=$(echo "$files_json" | jq -r '.files[] | select(.format | test("mp3"; "i")) | .name')
fi

# If no MP3, try Ogg
if [ -z "$files" ]; then
    files=$(echo "$files_json" | jq -r '.files[] | select(.format | test("ogg"; "i")) | .name')
fi

if [ -z "$files" ]; then
  echo "PROG:1.0:Error: No supported audio files found"
  exit 1
fi

IFS=$'\n'
file_array=($files)
total=${#file_array[@]}

if [ "$total" -eq 0 ]; then
  echo "PROG:1.0:Error: No audio files found"
  exit 1
fi

cur=0
for f in "${file_array[@]}"; do
  cur=$((cur+1))
  pct=$(awk "BEGIN {print $cur / $total}")
  name=$(basename "$f")
  f_url=$(echo -n "$f" | jq -sRr @uri)
  echo "PROG:$pct:Downloading $cur/$total: $name..."
  curl -L "https://archive.org/download/$id/$f_url" -o "$dir/$name" -s
done

echo "PROG:1.0:Download complete!"
EOF
chmod +x AppDir/usr/bin/sonora_archive.sh

if [ ! -f linuxdeploy-x86_64.AppImage ]; then
    echo "Downloading linuxdeploy..."
    wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x linuxdeploy-x86_64.AppImage
fi

if [ ! -f linuxdeploy-plugin-gtk.sh ]; then
    echo "Downloading linuxdeploy gtk plugin..."
    wget -q https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh
    chmod +x linuxdeploy-plugin-gtk.sh
fi

export APPIMAGE_EXTRACT_AND_RUN=1
export OUTPUT="${APP_NAME}-x86_64.AppImage"
export DEPLOY_GTK_VERSION=4
export NO_STRIP=1

echo "Running linuxdeploy..."
./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin gtk -d sonora.desktop -i sonora.svg -e AppDir/usr/bin/sonora

echo "Removing bundled sqlite3 to prevent ICU symbol conflicts..."
rm -fv AppDir/usr/lib/libsqlite3.so* AppDir/usr/lib64/libsqlite3.so*

echo "Packaging AppImage..."
export LDAI_OUTPUT="$OUTPUT"
./linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage
