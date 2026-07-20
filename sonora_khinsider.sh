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
