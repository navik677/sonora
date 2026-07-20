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
  echo "PROG:$pct:Downloading $cur/$total: $name..."
  # Use --create-dirs and -o just to be safe, but -O is easier if we cd
  curl -L "https://archive.org/download/$id/$f" -o "$dir/$name" -s
done

echo "PROG:1.0:Download complete!"
