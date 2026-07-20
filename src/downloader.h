#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <stdbool.h>

typedef struct {
    char id[32];
    char title[256];
    char artist[126];
    char duration_str[16];
    char thumbnail_url[512];
} SearchResult;

// Cover downloader
typedef void (*CoverCallback)(const char *cover_path, void *user_data);
void downloader_download_cover_async(const char *artist, const char *album, const char *title, const char *dest_dir, CoverCallback callback, void *user_data);

// Music downloader
typedef void (*SearchCallback)(SearchResult *results, int count, void *user_data);
void downloader_search_async(const char *query, SearchCallback callback, void *user_data);

typedef void (*DownloadCallback)(float progress, const char *status, bool finished, bool success, void *user_data);
void downloader_download_track_async(const char *video_id, const char *dest_dir, DownloadCallback callback, void *user_data);

#endif // DOWNLOADER_H
