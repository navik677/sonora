#include "downloader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <unistd.h>
#include <glib.h>

// Helper to escape URLs
static char *url_encode(CURL *curl, const char *str) {
    char *escaped = curl_easy_escape(curl, str, 0);
    char *result = strdup(escaped);
    curl_free(escaped);
    return result;
}

// Simple JSON extraction helper
static bool get_json_string(const char *json, const char *key, char *out, int max_len) {
    char key_pattern[128];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"", key);
    const char *pos = strstr(json, key_pattern);
    if (!pos) return false;
    
    // Find colon
    pos = strchr(pos + strlen(key_pattern), ':');
    if (!pos) return false;
    pos++;
    
    // Skip spaces
    while (*pos == ' ' || *pos == '\t') pos++;
    
    if (*pos == '"') {
        pos++; // start of string value
        int idx = 0;
        while (*pos && *pos != '"' && idx < max_len - 1) {
            if (*pos == '\\' && *(pos + 1) == '"') {
                out[idx++] = '"';
                pos += 2;
            } else {
                out[idx++] = *pos++;
            }
        }
        out[idx] = '\0';
        return true;
    } else {
        // numeric value
        int idx = 0;
        while (*pos && *pos != ',' && *pos != '}' && *pos != '\n' && idx < max_len - 1) {
            out[idx++] = *pos++;
        }
        out[idx] = '\0';
        // trim spaces
        while (idx > 0 && (out[idx-1] == ' ' || out[idx-1] == '\r')) {
            out[--idx] = '\0';
        }
        return true;
    }
}

// Curl write callback to write to a string
struct MemoryBuffer {
    char *data;
    size_t size;
};

static size_t curl_write_memory(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}

// Curl write callback to write to a file
static size_t curl_write_file(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

// ==========================================
// Cover Art Downloader
// ==========================================
typedef struct {
    char *artist;
    char *album;
    char *title;
    char *dest_dir;
    CoverCallback callback;
    void *user_data;
} CoverRequest;

typedef struct {
    char *cover_path;
    CoverCallback callback;
    void *user_data;
} CoverResponse;

static gboolean cover_idle_callback(gpointer user_data) {
    CoverResponse *res = (CoverResponse *)user_data;
    res->callback(res->cover_path, res->user_data);
    free(res->cover_path);
    free(res);
    return FALSE;
}

static void *cover_thread_func(void *arg) {
    CoverRequest *req = (CoverRequest *)arg;
    CURL *curl = curl_easy_init();
    char *final_cover_path = NULL;
    
    if (curl) {
        char *esc_artist = url_encode(curl, req->artist);
        char *esc_album = url_encode(curl, req->album);
        char *esc_title = req->title ? url_encode(curl, req->title) : NULL;
        
        bool is_unknown_artist = (!req->artist || strlen(req->artist) == 0 || strcasecmp(req->artist, "Unknown Artist") == 0);
        bool is_generic_album = (!req->album || strlen(req->album) == 0 || strcasecmp(req->album, "Unknown Album") == 0 || strcasecmp(req->album, "Sonora Downloads") == 0);
        unsigned int file_hash = g_str_hash(g_strdup_printf("%s_%s_%s", req->artist, req->album, req->title ? req->title : ""));

        char url[1024];
        if (is_generic_album && esc_title) {
            snprintf(url, sizeof(url), "https://itunes.apple.com/search?term=%s+%s&entity=song&limit=1", is_unknown_artist ? "" : esc_artist, esc_title);
        } else {
            snprintf(url, sizeof(url), "https://itunes.apple.com/search?term=%s+%s&entity=album&limit=1", is_unknown_artist ? "" : esc_artist, esc_album);
        }
        
        struct MemoryBuffer chunk = {NULL, 0};
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_memory);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 SonoraPlayer/1.0");
        
        char artwork_url[512] = {0};
            CURLcode rc = curl_easy_perform(curl);
            if (rc == CURLE_OK && chunk.data) {
                if (get_json_string(chunk.data, "artworkUrl100", artwork_url, sizeof(artwork_url))) {
                    char *replace_pos = strstr(artwork_url, "100x100bb.jpg");
                    if (replace_pos) memcpy(replace_pos, "600x600bb.jpg", 13);
                }
            }
            if (chunk.data) free(chunk.data);
            chunk.data = NULL;
            chunk.size = 0;
            
            // Fallback 1: iTunes album only
            if (!artwork_url[0]) {
                char url_fallback[1024];
                if (is_generic_album && esc_title) {
                    snprintf(url_fallback, sizeof(url_fallback), "https://itunes.apple.com/search?term=%s&entity=song&limit=1", esc_title);
                } else {
                    snprintf(url_fallback, sizeof(url_fallback), "https://itunes.apple.com/search?term=%s&entity=album&limit=1", esc_album);
                }
                curl_easy_setopt(curl, CURLOPT_URL, url_fallback);
                rc = curl_easy_perform(curl);
                if (rc == CURLE_OK && chunk.data) {
                    if (get_json_string(chunk.data, "artworkUrl100", artwork_url, sizeof(artwork_url))) {
                        char *replace_pos = strstr(artwork_url, "100x100bb.jpg");
                        if (replace_pos) memcpy(replace_pos, "600x600bb.jpg", 13);
                    }
                }
                if (chunk.data) free(chunk.data);
                chunk.data = NULL;
                chunk.size = 0;
            }
            
            // Fallback 2: MusicBrainz
            if (!artwork_url[0]) {
                char url_mb[1024];
                if (is_generic_album && esc_title) {
                    snprintf(url_mb, sizeof(url_mb), "https://musicbrainz.org/ws/2/recording?query=artist:\"%s\"%%20AND%%20recording:\"%s\"&fmt=json", esc_artist, esc_title);
                } else {
                    snprintf(url_mb, sizeof(url_mb), "https://musicbrainz.org/ws/2/release?query=artist:\"%s\"%%20AND%%20release:\"%s\"&fmt=json", esc_artist, esc_album);
                }
                curl_easy_setopt(curl, CURLOPT_URL, url_mb);
                rc = curl_easy_perform(curl);
                if (rc == CURLE_OK && chunk.data) {
                    const char *pos = strstr(chunk.data, "\"id\":");
                    if (pos) {
                        char mbid[64] = {0};
                        pos += 5; // skip "id":
                        while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == ':') pos++;
                        if (*pos == '"') {
                            pos++; // skip "
                            int idx = 0;
                            while (*pos && *pos != '"' && idx < 63) {
                                mbid[idx++] = *pos++;
                            }
                            mbid[idx] = '\0';
                            snprintf(artwork_url, sizeof(artwork_url), "http://coverartarchive.org/release/%s/front", mbid);
                        }
                    }
                }
                if (chunk.data) free(chunk.data);
                chunk.data = NULL;
                chunk.size = 0;
            }
        
        char local_file[1024];
        snprintf(local_file, sizeof(local_file), "%s/cover_%u.jpg", req->dest_dir, file_hash);
        
        if (artwork_url[0]) {
            FILE *fp = fopen(local_file, "wb");
            if (fp) {
                curl_easy_setopt(curl, CURLOPT_URL, artwork_url);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                if (curl_easy_perform(curl) == CURLE_OK) {
                    final_cover_path = strdup(local_file);
                }
                fclose(fp);
                if (!final_cover_path) remove(local_file);
            }
        }
        
        // Fallback 3: YouTube
        if (!final_cover_path && esc_title) {
            char yt_query[512];
            if (is_generic_album) {
                snprintf(yt_query, sizeof(yt_query), "%s %s", is_unknown_artist ? "" : req->artist, req->title);
            } else {
                snprintf(yt_query, sizeof(yt_query), "%s %s %s", is_unknown_artist ? "" : req->artist, req->album, req->title);
            }
            
            char esc_query[1024] = {0};
            int j = 0;
            for (int i = 0; yt_query[i] && j < 1000; i++) {
                if (yt_query[i] == '"') esc_query[j++] = '\\';
                esc_query[j++] = yt_query[i];
            }
            
            char cmd[4096];
            snprintf(cmd, sizeof(cmd), "yt-dlp --dump-json \"ytsearch1:%s\"", esc_query);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                char *line = NULL;
                size_t len = 0;
                if (getline(&line, &len, fp) != -1) {
                    char thumbnail_url[512] = {0};
                    get_json_string(line, "thumbnail", thumbnail_url, sizeof(thumbnail_url));
                    if (thumbnail_url[0]) {
                        FILE *out_fp = fopen(local_file, "wb");
                        if (out_fp) {
                            curl_easy_setopt(curl, CURLOPT_URL, thumbnail_url);
                            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
                            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file);
                            curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_fp);
                            if (curl_easy_perform(curl) == CURLE_OK) {
                                final_cover_path = strdup(local_file);
                            }
                            fclose(out_fp);
                            if (!final_cover_path) remove(local_file);
                        }
                    }
                }
                free(line);
                pclose(fp);
            }
        }
        
        free(esc_artist);
        free(esc_album);
        if (esc_title) free(esc_title);
        curl_easy_cleanup(curl);
    }
    
    // Dispatch to UI thread
    CoverResponse *res = malloc(sizeof(CoverResponse));
    res->cover_path = final_cover_path; // NULL if failed
    res->callback = req->callback;
    res->user_data = req->user_data;
    
    g_idle_add(cover_idle_callback, res);
    
    free(req->artist);
    free(req->album);
    if (req->title) free(req->title);
    free(req->dest_dir);
    free(req);
    return NULL;
}

void downloader_download_cover_async(const char *artist, const char *album, const char *title, const char *dest_dir, CoverCallback callback, void *user_data) {
    CoverRequest *req = malloc(sizeof(CoverRequest));
    req->artist = strdup(artist && *artist ? artist : "Unknown Artist");
    req->album = strdup(album && *album ? album : "Unknown Album");
    req->title = title && *title ? strdup(title) : NULL;
    req->dest_dir = strdup(dest_dir);
    req->callback = callback;
    req->user_data = user_data;
    
    pthread_t thread;
    pthread_create(&thread, NULL, cover_thread_func, req);
    pthread_detach(thread);
}

// ==========================================
// Music Search Downloader
// ==========================================
typedef struct {
    char query[256];
    SearchCallback callback;
    void *user_data;
} SearchRequest;

typedef struct {
    SearchResult *results;
    int count;
    SearchCallback callback;
    void *user_data;
} SearchResponse;

static gboolean search_idle_callback(gpointer user_data) {
    SearchResponse *res = (SearchResponse *)user_data;
    res->callback(res->results, res->count, res->user_data);
    free(res->results);
    free(res);
    return FALSE;
}

static void *search_thread_func(void *arg) {
    SearchRequest *req = (SearchRequest *)arg;
    
    // Escape double quotes in query
    char esc_query[512] = {0};
    int j = 0;
    for (int i = 0; req->query[i] && j < 500; i++) {
        if (req->query[i] == '"') {
            esc_query[j++] = '\\';
        }
        esc_query[j++] = req->query[i];
    }

    char cmd[4096];
    if (strncmp(req->query, "http", 4) == 0) {
        snprintf(cmd, sizeof(cmd), "yt-dlp --dump-json \"%s\"", esc_query);
    } else {
        snprintf(cmd, sizeof(cmd), "yt-dlp --dump-json \"ytsearch5:%s\"", esc_query);
    }
    
    FILE *fp = popen(cmd, "r");
    SearchResult *results = NULL;
    int results_count = 0;
    
    if (fp) {
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        
        int capacity = 5;
        results = malloc(capacity * sizeof(SearchResult));
        
        while ((read = getline(&line, &len, fp)) != -1) {
            if (results_count < 50) {
                char id[32], title[256], uploader[256], thumb[512], duration[32];
                get_json_string(line, "id", id, sizeof(id));
                get_json_string(line, "title", title, sizeof(title));
                get_json_string(line, "uploader", uploader, sizeof(uploader));
                if (!uploader[0]) get_json_string(line, "channel", uploader, sizeof(uploader));
                get_json_string(line, "duration_string", duration, sizeof(duration));
                get_json_string(line, "thumbnail", thumb, sizeof(thumb));

                if (results_count >= capacity) {
                    capacity *= 2;
                    results = realloc(results, capacity * sizeof(SearchResult));
                }
                
                char thumb_path[512];
                snprintf(thumb_path, sizeof(thumb_path), "/tmp/sonora_thumb_%s.jpg", id);
                if (access(thumb_path, F_OK) != 0 && strlen(thumb) > 0) {
                    char dl_cmd[4096];
                    snprintf(dl_cmd, sizeof(dl_cmd), "env -u LD_LIBRARY_PATH wget -q -O \"%s\" \"%s\"", thumb_path, thumb);
                    system(dl_cmd);
                }
                
                SearchResult *r = &results[results_count];
                memset(r, 0, sizeof(SearchResult));
                strncpy(r->id, id, sizeof(r->id)-1);
                strncpy(r->title, title, sizeof(r->title)-1);
                strncpy(r->artist, uploader, sizeof(r->artist)-1);
                strncpy(r->thumbnail_url, thumb_path, sizeof(r->thumbnail_url)-1);
                strncpy(r->duration_str, duration, sizeof(r->duration_str)-1);
                results_count++;
            }
        }
        free(line);
        pclose(fp);
    }
    
    SearchResponse *res = malloc(sizeof(SearchResponse));
    res->results = results;
    res->count = results_count;
    res->callback = req->callback;
    res->user_data = req->user_data;
    
    g_idle_add(search_idle_callback, res);
    
    free(req);
    return NULL;
}

void downloader_search_async(const char *query, SearchCallback callback, void *user_data) {
    SearchRequest *req = malloc(sizeof(SearchRequest));
    strncpy(req->query, query, sizeof(req->query) - 1);
    req->callback = callback;
    req->user_data = user_data;
    
    pthread_t thread;
    pthread_create(&thread, NULL, search_thread_func, req);
    pthread_detach(thread);
}

// ==========================================
// Track Downloader
// ==========================================
typedef struct {
    char video_id[32];
    char dest_dir[512];
    DownloadCallback callback;
    void *user_data;
} DownloadRequest;

typedef struct {
    float progress;
    char status[256];
    bool finished;
    bool success;
    DownloadCallback callback;
    void *user_data;
} DownloadProgressUpdate;

static gboolean download_idle_callback(gpointer user_data) {
    DownloadProgressUpdate *up = (DownloadProgressUpdate *)user_data;
    up->callback(up->progress, up->status, up->finished, up->success, up->user_data);
    free(up);
    return FALSE;
}

static void send_download_progress(float progress, const char *status, bool finished, bool success, DownloadCallback callback, void *user_data) {
    DownloadProgressUpdate *up = malloc(sizeof(DownloadProgressUpdate));
    up->progress = progress;
    strncpy(up->status, status, sizeof(up->status) - 1);
    up->finished = finished;
    up->success = success;
    up->callback = callback;
    up->user_data = user_data;
    g_idle_add(download_idle_callback, up);
}

static void *download_thread_func(void *arg) {
    DownloadRequest *req = (DownloadRequest *)arg;
    
    send_download_progress(0.0f, "Starting download...", false, false, req->callback, req->user_data);
    
    char cmd[4096];
    // Download audio, convert to FLAC (best quality), embed metadata, and save in dest_dir
    if (strncmp(req->video_id, "http", 4) == 0 || strncmp(req->video_id, "ytsearch", 8) == 0) {
        snprintf(cmd, sizeof(cmd), "yt-dlp -x --audio-format flac --audio-quality 0 --embed-metadata --embed-thumbnail -o \"%s/%%(title)s.%%(ext)s\" \"%s\"", req->dest_dir, req->video_id);
    } else {
        snprintf(cmd, sizeof(cmd), "yt-dlp -x --audio-format flac --audio-quality 0 --embed-metadata --embed-thumbnail -o \"%s/%%(title)s.%%(ext)s\" \"https://www.youtube.com/watch?v=%s\"", req->dest_dir, req->video_id);
    }
    FILE *fp = popen(cmd, "r");
    bool success = false;
    
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            // Check if there is percentage output
            // [download]  45.3% of ...
            char *p = strstr(line, "[download]");
            if (p) {
                float pct = 0.0f;
                if (sscanf(p, "[download] %f%%", &pct) == 1) {
                    char status[128];
                    snprintf(status, sizeof(status), "Downloading: %.1f%%", pct);
                    send_download_progress(pct / 100.0f, status, false, false, req->callback, req->user_data);
                }
            } else if (strstr(line, "[ExtractAudio]")) {
                send_download_progress(0.95f, "Converting to FLAC...", false, false, req->callback, req->user_data);
            }
        }
        int status = pclose(fp);
        success = (status == 0);
    }
    
    if (success) {
        send_download_progress(1.0f, "Completed!", true, true, req->callback, req->user_data);
    } else {
        send_download_progress(0.0f, "Download failed.", true, false, req->callback, req->user_data);
    }
    
    free(req);
    return NULL;
}

void downloader_download_track_async(const char *video_id, const char *dest_dir, DownloadCallback callback, void *user_data) {
    DownloadRequest *req = malloc(sizeof(DownloadRequest));
    strncpy(req->video_id, video_id, sizeof(req->video_id) - 1);
    strncpy(req->dest_dir, dest_dir, sizeof(req->dest_dir) - 1);
    req->callback = callback;
    req->user_data = user_data;
    
    pthread_t thread;
    pthread_create(&thread, NULL, download_thread_func, req);
    pthread_detach(thread);
}
