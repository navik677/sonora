#include "ui.h"

static void gtk_widget_set_padding(GtkWidget *w, int start, int end, int top, int bottom) {
    gtk_widget_set_margin_start(w, start);
    gtk_widget_set_margin_end(w, end);
    gtk_widget_set_margin_top(w, top);
    gtk_widget_set_margin_bottom(w, bottom);
}

#include "audio.h"
#include "db.h"
#include "downloader.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sndfile.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <libgen.h>

// Global UI Widgets
static GtkWidget *main_window = NULL;
static GtkWidget *flow_albums = NULL;
static GtkWidget *list_album_tracks = NULL;
static GtkWidget *list_playlist = NULL;
static GtkWidget *list_genres = NULL;

static GtkWidget *lbl_album_details_title = NULL;
static GtkWidget *lbl_track_title = NULL;
static GtkWidget *lbl_track_artist = NULL;
static GtkWidget *lbl_track_album = NULL;
static GtkWidget *lbl_track_genre = NULL;
static GtkWidget *lbl_track_year = NULL;
static GtkWidget *lbl_track_sr = NULL;
static GtkWidget *lbl_track_ch = NULL;
static GtkWidget *lbl_track_bitrate = NULL;
static GtkWidget *lbl_track_size = NULL;
static GtkWidget *lbl_track_path = NULL;
static GtkWidget *lbl_time_elapsed = NULL;
static GtkWidget *lbl_time_total = NULL;
static GtkWidget *scale_seek = NULL;
static GtkWidget *scale_volume = NULL;
static GtkWidget *img_cover_left = NULL;
static GtkWidget *btn_play_pause = NULL;
static GtkWidget *btn_mid_play_pause = NULL;
static GtkWidget *drawing_area_visualizer = NULL;

static GtkWidget *lbl_hdr_title = NULL;
static GtkWidget *lbl_hdr_artist = NULL;
static GtkWidget *lbl_hdr_album = NULL;
static GtkWidget *lbl_hdr_format = NULL;
static GtkWidget *lbl_hdr_bitrate = NULL;
static GtkWidget *lbl_hdr_path = NULL;

static GtkWidget *eq_sliders[15] = {NULL};

typedef struct {
    const char *name;
    bool is_dark;
    const char *bg_base;
    const char *bg_panel;
    const char *bg_sidebar;
    const char *bg_sidebar_btn;
    const char *bg_list;
    const char *bg_hover;
    const char *bg_trough;
    const char *border_color;
    const char *fg_primary;
    const char *fg_secondary;
    const char *accent_color;
    // gradient values for Cairo visualizer (R G B for top and bottom)
    float acc_r_top, acc_g_top, acc_b_top;
    float acc_r_bot, acc_g_bot, acc_b_bot;
} Theme;

static Theme themes[] = {
    // 0: Dark Orange (Default)
    {"Dark Orange", true,
     "#262626", "#303030", "#1e1e1e", "#242424", "#222222", "#2a2a2a", "#404040", "#1a1a1a", "#ffffff", "#c0c0c0", "#ff9800",
     1.0f, 0.6f, 0.0f,   1.0f, 0.1f, 0.1f},
    // 1: Dark Green
    {"Dark Green", true,
     "#262626", "#303030", "#1e1e1e", "#242424", "#222222", "#2a2a2a", "#404040", "#1a1a1a", "#ffffff", "#c0c0c0", "#8bc34a",
     0.6f, 1.0f, 0.2f,   0.2f, 0.5f, 0.1f},
    // 2: Dark Blue
    {"Dark Blue", true,
     "#262626", "#303030", "#1e1e1e", "#242424", "#222222", "#2a2a2a", "#404040", "#1a1a1a", "#ffffff", "#c0c0c0", "#2196f3",
     0.2f, 0.6f, 1.0f,   0.0f, 0.2f, 0.8f},
    // 3: Light Orange
    {"Light Orange", false,
     "#ffffff", "#f0f0f0", "#e8e8e8", "#e0e0e0", "#ffffff", "#e5e5e5", "#d0d0d0", "#cccccc", "#1a1a1a", "#505050", "#ff9800",
     1.0f, 0.6f, 0.0f,   1.0f, 0.2f, 0.0f},
    // 4: Light Green
    {"Light Green", false,
     "#ffffff", "#f0f0f0", "#e8e8e8", "#e0e0e0", "#ffffff", "#e5e5e5", "#d0d0d0", "#cccccc", "#1a1a1a", "#505050", "#4caf50",
     0.4f, 0.8f, 0.2f,   0.1f, 0.5f, 0.1f},
    // 5: Light Blue
    {"Light Blue", false,
     "#ffffff", "#f0f0f0", "#e8e8e8", "#e0e0e0", "#ffffff", "#e5e5e5", "#d0d0d0", "#cccccc", "#1a1a1a", "#505050", "#2196f3",
     0.2f, 0.6f, 1.0f,   0.0f, 0.3f, 0.8f}
};
static int current_theme_index = 0;
static GtkCssProvider *theme_provider = NULL;

static void apply_theme(int index);
static GtkWidget *eq_preamp_slider = NULL;



// Streams Tab Widgets
static GtkWidget *entry_stream_url = NULL;
static GtkWidget *btn_dl_khinsider = NULL;
static GtkWidget *btn_dl_archive = NULL;
static GtkWidget *lbl_stream_status = NULL;
static GtkWidget *prog_stream = NULL;

static GtkWidget *eq_band_val_labels[15] = {NULL};
static GtkWidget *eq_preamp_val_label = NULL;

static GtkWidget *box_search_results = NULL;
static GtkWidget *progress_download = NULL;
static GtkWidget *lbl_download_status = NULL;

static GtkWidget *box_folders_list = NULL;
static GtkWidget *txt_lib_search = NULL;
static GtkWidget *list_all_tracks = NULL;
static GtkWidget *lib_sub_stack = NULL;
static char **all_tracks_table_paths = NULL;
static int all_tracks_table_count = 0;

// Playlist State (Left Panel Queue)
static char **playlist_paths = NULL;
static int playlist_count = 0;
static int playlist_index = -1;
static bool playlist_shuffle = false;
static bool playlist_loop = false;



// Filter state
static char *current_selected_genre = NULL;

// Active download details
static char active_download_id[32] = {0};

// Directories & DB
static const char *music_dir = "/home/ona/.gemini/antigravity/scratch/sonora/music";
static char covers_dir[1024];
static char db_file[1024];

// Forward declarations
static void load_album_tracks(const char *album);
static void play_playlist_track_at_index(int idx);

static void apply_theme(int index) {
    if (index < 0 || index >= (int)(sizeof(themes) / sizeof(themes[0]))) return;
    current_theme_index = index;
    Theme *t = &themes[index];

    // Update GTK settings
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", t->is_dark, NULL);

    // Load style.css from resource
    GBytes *bytes = g_resources_lookup_data("/org/sonora/style.css", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    gsize style_len = 0;
    const char *style_data = "";
    if (bytes) {
        style_data = g_bytes_get_data(bytes, &style_len);
    }

    // Generate CSS
    char *css = g_strdup_printf(
        "@define-color bg_base %s;\n"
        "@define-color bg_panel %s;\n"
        "@define-color bg_sidebar %s;\n"
        "@define-color bg_sidebar_btn %s;\n"
        "@define-color bg_list %s;\n"
        "@define-color bg_hover %s;\n"
        "@define-color bg_trough %s;\n"
        "@define-color border_color %s;\n"
        "@define-color fg_primary %s;\n"
        "@define-color fg_secondary %s;\n"
        "@define-color accent_color %s;\n"
        "%.*s\n",
        t->bg_base, t->bg_panel, t->bg_sidebar, t->bg_sidebar_btn,
        t->bg_list, t->bg_hover, t->bg_trough, t->border_color,
        t->fg_primary, t->fg_secondary, t->accent_color,
        (int)style_len, style_data
    );

    if (bytes) {
        g_bytes_unref(bytes);
    }

    if (!theme_provider) {
        theme_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                                   GTK_STYLE_PROVIDER(theme_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
    
    // DEBUG: Write to file
    FILE *df = fopen("/home/ona/.gemini/antigravity/scratch/sonora_generated.css", "w");
    if (df) {
        fputs(css, df);
        fclose(df);
    }
    
    gtk_css_provider_load_from_string(theme_provider, css);
    g_free(css);
}

static void on_theme_selected(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    (void)user_data;
    int idx = gtk_drop_down_get_selected(dropdown);
    apply_theme(idx);
    
    char idx_str[16];
    snprintf(idx_str, sizeof(idx_str), "%d", idx);
    db_set_setting("theme_index", idx_str);
}
static void refresh_folders_list(void);
static void refresh_genres_list(void);
static void ui_refresh_all(void);
static void ui_refresh_all_tracks(void);
static void load_album_tracks(const char *album);
static void on_download_progress_update(float progress, const char *status, bool finished, bool success, void *user_data);
static void *yt_stream_thread(void *arg);

// ==========================================
// Streams Tab Logic
// ==========================================

typedef struct {
    char cmd[4096];
} ScriptRequest;

typedef struct {
    float progress;
    char status[256];
} ScriptUpdate;

static gboolean script_progress_idle(gpointer user_data) {
    ScriptUpdate *up = (ScriptUpdate *)user_data;
    if (lbl_stream_status) gtk_label_set_text(GTK_LABEL(lbl_stream_status), up->status);
    if (prog_stream) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(prog_stream), up->progress);
    free(up);
    return FALSE;
}

static void *script_thread_func(void *arg) {
    ScriptRequest *req = (ScriptRequest *)arg;
    FILE *fp = popen(req->cmd, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            // Check for PROG:pct:status
            if (strncmp(line, "PROG:", 5) == 0) {
                float pct = 0.0f;
                char status[256] = {0};
                if (sscanf(line, "PROG:%f:%255[^\n]", &pct, status) >= 1) {
                    ScriptUpdate *up = malloc(sizeof(ScriptUpdate));
                    up->progress = pct;
                    strncpy(up->status, status, sizeof(up->status) - 1);
                    g_idle_add(script_progress_idle, up);
                }
            } else {
                // If not PROG format, just print to console for debugging
                printf("[Script] %s", line);
            }
        }
        pclose(fp);
    }
    free(req);
    return NULL;
}

static void spawn_download_script(const char *script_name, const char *url) {
    const char *music_dir = g_get_user_special_dir(G_USER_DIRECTORY_MUSIC);
    if (!music_dir) music_dir = "/tmp";
    
    // Resolve script path relative to the executable directory
    char script_path[1024];
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        char *dir = dirname(exe_path);
        snprintf(script_path, sizeof(script_path), "%s/%s", dir, script_name);
    } else {
        strncpy(script_path, script_name, sizeof(script_path)-1);
    }
    
    ScriptRequest *req = malloc(sizeof(ScriptRequest));
    snprintf(req->cmd, sizeof(req->cmd), "\"%s\" \"%s\" \"%s/Sonora Downloads\"", script_path, url, music_dir);
    
    if (lbl_stream_status) gtk_label_set_text(GTK_LABEL(lbl_stream_status), "Starting...");
    if (prog_stream) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(prog_stream), 0.0);
    
    pthread_t thread;
    pthread_create(&thread, NULL, script_thread_func, req);
    pthread_detach(thread);
}

static void on_btn_dl_khinsider_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    const char *url = gtk_editable_get_text(GTK_EDITABLE(entry_stream_url));
    if (url && strlen(url) > 0) {
        spawn_download_script("sonora_khinsider.sh", url);
    }
}

static void on_btn_dl_archive_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    const char *url = gtk_editable_get_text(GTK_EDITABLE(entry_stream_url));
    if (url && strlen(url) > 0) {
        spawn_download_script("sonora_archive.sh", url);
    }
}



typedef struct {
    char url[512];
} StreamReq;

static void on_btn_stream_result_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    const char *url = (const char *)g_object_get_data(G_OBJECT(btn), "video_id");
    if (url && strlen(url) > 0) {
        StreamReq *req = malloc(sizeof(StreamReq));
        if (strncmp(url, "http", 4) != 0) {
            snprintf(req->url, sizeof(req->url), "https://www.youtube.com/watch?v=%s", url);
        } else {
            strncpy(req->url, url, sizeof(req->url)-1);
        }
        pthread_t t;
        pthread_create(&t, NULL, yt_stream_thread, req);
        pthread_detach(t);
    }
}

static void *yt_stream_thread(void *arg) {
    StreamReq *req = (StreamReq *)arg;
    char cmd[4096];
    
    ScriptUpdate *up = malloc(sizeof(ScriptUpdate));
    up->progress = 0.2;
    strncpy(up->status, "Extracting stream URL (yt-dlp)...", sizeof(up->status)-1);
    g_idle_add(script_progress_idle, up);
    
    if (strncmp(req->url, "http", 4) == 0 || strncmp(req->url, "ytsearch", 8) == 0) {
        snprintf(cmd, sizeof(cmd), "yt-dlp -f bestaudio -g \"%s\"", req->url);
    } else {
        snprintf(cmd, sizeof(cmd), "yt-dlp -f bestaudio -g \"ytsearch1:%s\"", req->url);
    }
    FILE *fp = popen(cmd, "r");
    char stream_url[2048] = {0};
    if (fp) {
        fgets(stream_url, sizeof(stream_url), fp);
        pclose(fp);
    }
    if (strlen(stream_url) > 0 && stream_url[strlen(stream_url)-1] == '\n') {
        stream_url[strlen(stream_url)-1] = '\0';
    }
    
    if (strlen(stream_url) == 0) {
        ScriptUpdate *fail = malloc(sizeof(ScriptUpdate));
        fail->progress = 1.0;
        strncpy(fail->status, "Failed to extract YouTube stream.", sizeof(fail->status)-1);
        g_idle_add(script_progress_idle, fail);
        free(req);
        return NULL;
    }
    
    ScriptUpdate *up2 = malloc(sizeof(ScriptUpdate));
    up2->progress = 0.6;
    strncpy(up2->status, "Buffering stream (ffmpeg)...", sizeof(up2->status)-1);
    g_idle_add(script_progress_idle, up2);
    
    system("rm -f /tmp/sonora_stream.wav && mkfifo /tmp/sonora_stream.wav");
    snprintf(cmd, sizeof(cmd), "env -u LD_LIBRARY_PATH ffmpeg -y -i \"%s\" -f wav /tmp/sonora_stream.wav > /tmp/sonora_ffmpeg.log 2>&1 &", stream_url);
    system(cmd);
    
    ScriptUpdate *up3 = malloc(sizeof(ScriptUpdate));
    up3->progress = 1.0;
    strncpy(up3->status, "Playing stream!", sizeof(up3->status)-1);
    g_idle_add(script_progress_idle, up3);
    
    // Small delay to ensure ffmpeg has opened the pipe for writing before sndfile reads it
    usleep(500000); 
    
    audio_play_file("/tmp/sonora_stream.wav");
    
    free(req);
    return NULL;
}

static void on_btn_stream_yt_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    const char *url = gtk_editable_get_text(GTK_EDITABLE(entry_stream_url));
    if (url && strlen(url) > 0) {
        StreamReq *req = malloc(sizeof(StreamReq));
        strncpy(req->url, url, sizeof(req->url)-1);
        pthread_t t;
        pthread_create(&t, NULL, yt_stream_thread, req);
        pthread_detach(t);
    }
}

static void ui_refresh_albums(void);
static void open_folder_chooser(GtkWidget *parent);
static void on_visualizer_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    (void)drawing_area;
    (void)user_data;

    // Draw dark background (optional if CSS handles it, but let's be safe or just let CSS do it)
    // Actually CSS handles background.



    // Read FFT bins from audio engine
    float fft[24];
    pthread_mutex_lock(&g_audio.mutex);
    for (int i = 0; i < 24; i++) {
        fft[i] = g_audio.fft_bins[i];
    }
    pthread_mutex_unlock(&g_audio.mutex);
    
    // Smooth bins
    static float smooth_bins[24] = {0};
    for (int i = 0; i < 24; i++) {
        if (fft[i] > smooth_bins[i]) {
            smooth_bins[i] = fft[i]; // Instant attack
        } else {
            smooth_bins[i] -= 0.05f; // Decay
            if (smooth_bins[i] < 0) smooth_bins[i] = 0;
        }
    }

    int num_bars = 24;
    float bar_width = (width - 20.0f) / num_bars;
    
    // Draw stacked dashes with gradient
    Theme *t = &themes[current_theme_index];
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgb(pat, 0.0, t->acc_r_top, t->acc_g_top, t->acc_b_top);
    cairo_pattern_add_color_stop_rgb(pat, 1.0, t->acc_r_bot, t->acc_g_bot, t->acc_b_bot);
    cairo_set_source(cr, pat);
    
    for (int i = 0; i < num_bars; i++) {
        float val = smooth_bins[i];
        if (val > 1.0f) val = 1.0f;
        float bar_h = height * val;
        if (bar_h < 5) bar_h = 5;
        
        int num_dashes = (int)(bar_h / 4.0f);
        float x = 10.0f + i * bar_width;
        
        for (int d = 0; d < num_dashes; d++) {
            float y = height - (d * 4.0f) - 2.0f;
            cairo_rectangle(cr, x + 1, y, bar_width - 2, 2);
            cairo_fill(cr);
        }
        // Peak cap
        cairo_rectangle(cr, x + 1, height - bar_h - 4.0f, bar_width - 2, 2);
        cairo_fill(cr);
    }
    
    cairo_pattern_destroy(pat);
}


static GdkTexture *create_flat_color_texture(int width, int height, guint32 rgba) {
    int stride = width * 4;
    guchar *data = g_malloc(height * stride);
    guchar r = (rgba >> 24) & 0xff;
    guchar g = (rgba >> 16) & 0xff;
    guchar b = (rgba >> 8) & 0xff;
    guchar a = rgba & 0xff;
    for (int i = 0; i < width * height; i++) {
        data[i * 4] = r;
        data[i * 4 + 1] = g;
        data[i * 4 + 2] = b;
        data[i * 4 + 3] = a;
    }
    GBytes *bytes = g_bytes_new_take(data, height * stride);
    GdkTexture *tex = gdk_memory_texture_new(width, height, GDK_MEMORY_R8G8B8A8, bytes, stride);
    g_bytes_unref(bytes);
    return tex;
}

static void audio_set_band_gain(Equalizer *eq, int band, float gain_db) {
    pthread_mutex_lock(&g_audio.mutex);
    eq_set_band_gain(eq, band, gain_db);
    pthread_mutex_unlock(&g_audio.mutex);
}

static void audio_set_preamp(Equalizer *eq, float preamp_db) {
    pthread_mutex_lock(&g_audio.mutex);
    eq_set_preamp(eq, preamp_db);
    pthread_mutex_unlock(&g_audio.mutex);
}

static void gtk_flow_box_set_valign(GtkFlowBox *fb, GtkAlign align) {
    gtk_widget_set_valign(GTK_WIDGET(fb), align);
}

// Helper: check if file ends with suffix
static bool has_suffix(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    return (str_len >= suffix_len && strcasecmp(str + str_len - suffix_len, suffix) == 0);
}

// Helper: Extract filename without extension
static void get_basename_no_ext(const char *path, char *out, int max_len) {
    const char *base = strrchr(path, '/');
    if (!base) base = path;
    else base++;
    
    strncpy(out, base, max_len - 1);
    out[max_len - 1] = '\0';
    
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
}

// ==========================================
// Directory Scanner Implementation
// ==========================================
typedef struct {
    char path[512];
} ScanRequest;

static gboolean scan_finished_idle(gpointer user_data) {
    (void)user_data;
    ui_refresh_all();
    return FALSE;
}

static void on_cover_downloaded(const char *cover_path, void *user_data) {
    char *album = (char *)user_data;
    if (cover_path && album) {
        db_update_album_cover(album, cover_path);
        g_idle_add(scan_finished_idle, NULL);
    }
    free(album);
}

static void download_cover_finished_track(const char *cover_path, void *user_data) {
    char *full_path = (char *)user_data;
    if (cover_path && full_path) {
        db_update_track_cover_by_path(full_path, cover_path);
        g_idle_add(scan_finished_idle, NULL);
    }
    free(full_path);
}

static bool extract_embedded_cover(const char *audio_path, const char *out_img_path) {
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    } else if (pid == 0) {
        // Child process: Redirect stdout and stderr to /dev/null to make execution silent
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null >= 0) {
            dup2(dev_null, 1);
            dup2(dev_null, 2);
            close(dev_null);
        }
        
        char *args[] = {
            "ffmpeg",
            "-y",
            "-i",
            (char *)audio_path,
            "-an",
            "-vcodec",
            "copy",
            (char *)out_img_path,
            NULL
        };
        execvp("ffmpeg", args);
        exit(127);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            struct stat st;
            if (stat(out_img_path, &st) == 0 && st.st_size > 0) {
                return true;
            }
        }
    }
    return false;
}

static char *convert_to_utf8(const char *str) {
    if (!str || !*str) return NULL;
    if (g_utf8_validate(str, -1, NULL)) {
        return strdup(str);
    }
    gsize bytes_read = 0, bytes_written = 0;
    GError *err = NULL;
    char *utf8 = g_convert(str, -1, "UTF-8", "WINDOWS-1251", &bytes_read, &bytes_written, &err);
    if (utf8) {
        return utf8;
    }
    if (err) g_error_free(err);
    
    // If CP1251 failed, try ISO-8859-1 (Latin-1) as a last resort
    utf8 = g_convert(str, -1, "UTF-8", "ISO-8859-1", &bytes_read, &bytes_written, &err);
    if (utf8) return utf8;
    if (err) g_error_free(err);
    
    return strdup("???"); // Fallback to avoid GTK crashes on invalid UTF-8
}

static void scan_directory_recursive(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                scan_directory_recursive(full_path);
            } else if (S_ISREG(st.st_mode)) {
                const char *ext = strrchr(full_path, '.');
                bool is_audio = false;
                if (ext) {
                    if (strcasecmp(ext, ".flac") == 0 ||
                        strcasecmp(ext, ".mp3") == 0 ||
                        strcasecmp(ext, ".wav") == 0 ||
                        strcasecmp(ext, ".ogg") == 0 ||
                        strcasecmp(ext, ".m4a") == 0) {
                        is_audio = true;
                    }
                }

                if (is_audio) {
                    if (db_track_exists(full_path)) {
                        continue;
                    }
                    printf("Found audio: %s\n", full_path);
                    SF_INFO sf_info;
                    memset(&sf_info, 0, sizeof(sf_info));
                    SNDFILE *sf = sf_open(full_path, SFM_READ, &sf_info);
                    if (!sf) {
                        printf("Failed to open with libsndfile: %s\n", full_path);
                    }
                    if (sf) {
                        Track t;
                        memset(&t, 0, sizeof(t));
                        t.path = full_path;
                        t.duration = (double)sf_info.frames / sf_info.samplerate;
                        t.samplerate = sf_info.samplerate;
                        t.channels = sf_info.channels;
                        
                        char *title = convert_to_utf8(sf_get_string(sf, SF_STR_TITLE));
                        char *artist = convert_to_utf8(sf_get_string(sf, SF_STR_ARTIST));
                        char *album = convert_to_utf8(sf_get_string(sf, SF_STR_ALBUM));
                        char *genre = convert_to_utf8(sf_get_string(sf, SF_STR_GENRE));
                        const char *date = sf_get_string(sf, SF_STR_DATE);
                        
                        char base_name[256];
                        get_basename_no_ext(full_path, base_name, sizeof(base_name));
                        
                        if (!title || !*title) {
                            char *dash = strstr(base_name, " - ");
                            if (dash) {
                                t.title = strdup(dash + 3);
                            } else {
                                t.title = strdup(base_name);
                            }
                        } else {
                            t.title = strdup(title);
                        }
                        
                        if (!artist || !*artist) {
                            char *dash = strstr(base_name, " - ");
                            if (dash) {
                                *dash = '\0';
                                t.artist = strdup(base_name);
                            } else {
                                t.artist = strdup("Unknown Artist");
                            }
                        } else {
                            t.artist = strdup(artist);
                        }
                        
                        const char *parent_dir_name = strrchr(dir_path, '/');
                        if (parent_dir_name) parent_dir_name++;
                        else parent_dir_name = dir_path;
                        
                        t.album = strdup(album && *album ? album : parent_dir_name);
                        t.genre = strdup(genre && *genre ? genre : "Alternative");
                        t.year = date ? atoi(date) : 0;
                        
                        bool is_generic_album = (!album || strlen(album) == 0 || strcasecmp(t.album, "Unknown Album") == 0 || strcasecmp(t.album, "Sonora Downloads") == 0 || strstr(t.album, "未知") != NULL);

                        if (title) free(title);
                        if (artist) free(artist);
                        if (album) free(album);
                        if (genre) free(genre);
                        
                        char hash_str[1024];
                        if (is_generic_album) {
                            snprintf(hash_str, sizeof(hash_str), "%s_%s_%s", t.artist, t.album, t.title);
                        } else {
                            snprintf(hash_str, sizeof(hash_str), "%s", t.album);
                        }
                        guint cover_hash = g_str_hash(hash_str);

                        char *existing_cover = db_get_track_cover(full_path);
                        if (existing_cover && existing_cover[0] && access(existing_cover, F_OK) != 0) {
                            free(existing_cover);
                            existing_cover = NULL;
                        }

                        bool extracted = false;
                        if (existing_cover && existing_cover[0]) {
                            t.cover_path = existing_cover;
                        } else {
                            char target_cover[1024];
                            snprintf(target_cover, sizeof(target_cover), "%s/cover_%u.jpg", covers_dir, cover_hash);
                            if (extract_embedded_cover(full_path, target_cover)) {
                                t.cover_path = target_cover;
                                extracted = true;
                            } else {
                                t.cover_path = "";
                            }
                        }
                        
                        db_add_track(&t);
                        
                        if (extracted) {
                            if (!is_generic_album) {
                                db_update_album_cover(t.album, t.cover_path);
                            } else {
                                db_update_track_cover_by_path(full_path, t.cover_path);
                            }
                        } else if (!existing_cover || !existing_cover[0]) {
                            if (!is_generic_album) {
                                char *album_cover = db_get_album_cover(t.album);
                                if (album_cover && *album_cover && access(album_cover, F_OK) == 0) {
                                    db_update_album_cover(t.album, album_cover);
                                    free(album_cover);
                                } else {
                                    static GHashTable *requested = NULL;
                                    if (!requested) requested = g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);
                                    
                                    if (!g_hash_table_contains(requested, t.album)) {
                                        g_hash_table_add(requested, strdup(t.album));
                                        downloader_download_cover_async(t.artist, t.album, t.title, covers_dir, on_cover_downloaded, strdup(t.album));
                                    }
                                }
                            } else {
                                downloader_download_cover_async(t.artist, t.album, t.title, covers_dir, download_cover_finished_track, strdup(full_path));
                            }
                        }
                        
                        if (existing_cover) free(existing_cover);
                        free(t.title);
                        free(t.artist);
                        free(t.album);
                        free(t.genre);
                        sf_close(sf);
                    }
                }
            }
        }
    }
    closedir(dir);
}

static void *scan_thread_func(void *arg) {
    ScanRequest *req = (ScanRequest *)arg;
    scan_directory_recursive(req->path);
    g_idle_add(scan_finished_idle, NULL);
    free(req);
    return NULL;
}

static void trigger_scan(const char *path) {
    ScanRequest *req = malloc(sizeof(ScanRequest));
    strncpy(req->path, path, sizeof(req->path) - 1);
    
    pthread_t thread;
    pthread_create(&thread, NULL, scan_thread_func, req);
    pthread_detach(thread);
}

static bool g_is_scanning = false;

static void *global_scan_thread_func(void *arg) {
    (void)arg;
    int fc = 0;
    char **fols = db_get_folders(&fc);
    if (fols) {
        for (int i = 0; i < fc; i++) {
            scan_directory_recursive(fols[i]);
        }
        db_free_folders(fols, fc);
    }
    
    db_cleanup_missing_tracks();
    
    g_idle_add(scan_finished_idle, NULL);
    g_is_scanning = false;
    return NULL;
}

static void trigger_global_rescan(void) {
    if (g_is_scanning) return;
    g_is_scanning = true;
    
    pthread_t thread;
    pthread_create(&thread, NULL, global_scan_thread_func, NULL);
    pthread_detach(thread);
}

static void on_btn_lib_rescan_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    trigger_global_rescan();
}

static gboolean background_scan_timer(gpointer user_data) {
    (void)user_data;
    trigger_global_rescan();
    return G_SOURCE_CONTINUE;
}

// ==========================================
// Folder Chooser Dialog wrapper (Fixed: direct GtkFileChooserDialog)
// ==========================================
static void on_folder_dialog_response(GtkDialog *dialog, int response_id, gpointer user_data) {
    (void)user_data;
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        GFile *file = gtk_file_chooser_get_file(chooser);
        if (file) {
            char *path = g_file_get_path(file);
            if (path) {
                db_add_folder(path);
                trigger_scan(path);
                g_free(path);
            }
            g_object_unref(file);
        }
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void open_folder_chooser(GtkWidget *parent) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Select Music Directory",
                                                     GTK_WINDOW(parent),
                                                     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                     "_Cancel", GTK_RESPONSE_CANCEL,
                                                     "_Select", GTK_RESPONSE_ACCEPT,
                                                     NULL);
    g_signal_connect(dialog, "response", G_CALLBACK(on_folder_dialog_response), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

// ==========================================
// Playlist Queue Operations (Left Panel)
// ==========================================
static int parse_track_number_from_path(const char *path, int fallback_index) {
    if (!path) return fallback_index;
    const char *filename = strrchr(path, '/');
    if (filename) filename++;
    else filename = path;

    int num = 0;
    const char *p = filename;
    while (*p && (*p == ' ' || *p == '_' || *p == '-')) p++;
    
    if (g_ascii_isdigit(*p)) {
        num = atoi(p);
        if (num > 0) return num;
    }
    while (*p) {
        if (!g_ascii_isdigit(*p) && g_ascii_isdigit(*(p+1))) {
            num = atoi(p+1);
            if (num > 0) return num;
            break;
        }
        p++;
    }
    return fallback_index;
}

static void refresh_playlist_queue_ui(void) {
    if (!list_playlist) return;
    
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(list_playlist)) != NULL) {
        gtk_list_box_remove(GTK_LIST_BOX(list_playlist), child);
    }
    
    // 1. Header Row
    GtkWidget *hdr_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_add_css_class(hdr_box, "playlist-row-box");
    gtk_widget_add_css_class(hdr_box, "table-header-row");
    
    GtkWidget *lbl_h_id = gtk_label_new("ID");
    gtk_widget_set_size_request(lbl_h_id, 30, -1);
    gtk_widget_set_halign(lbl_h_id, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_id);
    
    GtkWidget *lbl_h_title = gtk_label_new("Track");
    gtk_widget_set_halign(lbl_h_title, GTK_ALIGN_START);
    gtk_widget_set_size_request(lbl_h_title, 150, -1);
    gtk_widget_set_hexpand(lbl_h_title, TRUE);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_title);
    
    GtkWidget *lbl_h_art = gtk_label_new("Artist");
    gtk_widget_set_halign(lbl_h_art, GTK_ALIGN_START);
    gtk_widget_set_size_request(lbl_h_art, 120, -1);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_art);
    
    GtkWidget *lbl_h_alb = gtk_label_new("Album");
    gtk_widget_set_halign(lbl_h_alb, GTK_ALIGN_START);
    gtk_widget_set_size_request(lbl_h_alb, 120, -1);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_alb);
    
    GtkWidget *lbl_h_dur = gtk_label_new("Time");
    gtk_widget_set_halign(lbl_h_dur, GTK_ALIGN_END);
    gtk_widget_set_size_request(lbl_h_dur, 50, -1);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_dur);
    
    GtkWidget *lbl_h_bit = gtk_label_new("Bitrate");
    gtk_widget_set_halign(lbl_h_bit, GTK_ALIGN_END);
    gtk_widget_set_size_request(lbl_h_bit, 70, -1);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_bit);
    
    GtkWidget *hdr_row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(hdr_row), FALSE);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(hdr_row), hdr_box);
    gtk_list_box_append(GTK_LIST_BOX(list_playlist), hdr_row);
    
    // 2. Data Rows
    for (int i = 0; i < playlist_count; i++) {
        char *path = playlist_paths[i];
        sqlite3 *db;
        char *title = NULL;
        char *artist = NULL;
        char *album = NULL;
        double duration = 0.0;
        
        if (sqlite3_open(db_file, &db) == SQLITE_OK) {
            sqlite3_stmt *stmt;
            const char *sql = "SELECT title, artist, album, duration FROM tracks WHERE path = ? LIMIT 1;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const unsigned char *t = sqlite3_column_text(stmt, 0);
                    if (t) title = strdup((const char*)t);
                    const unsigned char *a = sqlite3_column_text(stmt, 1);
                    if (a) artist = strdup((const char*)a);
                    const unsigned char *al = sqlite3_column_text(stmt, 2);
                    if (al) album = strdup((const char*)al);
                    duration = sqlite3_column_double(stmt, 3);
                }
                sqlite3_finalize(stmt);
            }
            sqlite3_close(db);
        }
        
        GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
        gtk_widget_add_css_class(row_box, "playlist-row-box");
        
        char idx_str[16];
        snprintf(idx_str, sizeof(idx_str), "%d", i + 1);
        GtkWidget *lbl_idx = gtk_label_new(idx_str);
        gtk_widget_set_size_request(lbl_idx, 30, -1);
        gtk_widget_set_halign(lbl_idx, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(row_box), lbl_idx);
        
        int track_num = parse_track_number_from_path(path, i + 1);
        char title_formatted[512];
        snprintf(title_formatted, sizeof(title_formatted), "%d  %s", track_num, title ? title : "Unknown Title");
        GtkWidget *lbl_t = gtk_label_new(title_formatted);
        gtk_widget_set_halign(lbl_t, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(lbl_t), PANGO_ELLIPSIZE_END);
        gtk_widget_set_size_request(lbl_t, 150, -1);
        gtk_widget_set_hexpand(lbl_t, TRUE);
        gtk_box_append(GTK_BOX(row_box), lbl_t);
        
        GtkWidget *lbl_art = gtk_label_new(artist ? artist : "Unknown Artist");
        gtk_widget_set_halign(lbl_art, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(lbl_art), PANGO_ELLIPSIZE_END);
        gtk_widget_set_size_request(lbl_art, 120, -1);
        gtk_box_append(GTK_BOX(row_box), lbl_art);
        
        GtkWidget *lbl_alb = gtk_label_new(album ? album : "Unknown Album");
        gtk_widget_set_halign(lbl_alb, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(lbl_alb), PANGO_ELLIPSIZE_END);
        gtk_widget_set_size_request(lbl_alb, 120, -1);
        gtk_box_append(GTK_BOX(row_box), lbl_alb);
        
        int mins = (int)duration / 60;
        int secs = (int)duration % 60;
        char dur_str[16];
        snprintf(dur_str, sizeof(dur_str), "%d:%02d", mins, secs);
        GtkWidget *lbl_d = gtk_label_new(dur_str);
        gtk_widget_set_halign(lbl_d, GTK_ALIGN_END);
        gtk_widget_set_size_request(lbl_d, 50, -1);
        gtk_box_append(GTK_BOX(row_box), lbl_d);
        
        double file_size_bytes = 0.0;
        struct stat st;
        if (stat(path, &st) == 0) {
            file_size_bytes = (double)st.st_size;
        }
        int actual_bitrate = 0;
        if (duration > 0.0) {
            actual_bitrate = (int)round((file_size_bytes * 8.0) / (duration * 1000.0));
        }
        char bit_str[32];
        snprintf(bit_str, sizeof(bit_str), "%d kbps", actual_bitrate);
        GtkWidget *lbl_bit = gtk_label_new(bit_str);
        gtk_widget_set_halign(lbl_bit, GTK_ALIGN_END);
        gtk_widget_set_size_request(lbl_bit, 70, -1);
        gtk_box_append(GTK_BOX(row_box), lbl_bit);
        
        GtkWidget *row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
        gtk_list_box_append(GTK_LIST_BOX(list_playlist), row);
        if (title) free(title);
        if (artist) free(artist);
        if (album) free(album);
    }
    
    if (playlist_index >= 0 && playlist_index < playlist_count) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_playlist), playlist_index + 1);
        if (row) {
            gtk_list_box_select_row(GTK_LIST_BOX(list_playlist), row);
        }
    }
}

static void on_playlist_row_activated(GtkListBox *listbox, GtkListBoxRow *row, gpointer user_data) {
    (void)listbox;
    (void)user_data;
    if (!row) return;
    int idx = gtk_list_box_row_get_index(row) - 1;
    play_playlist_track_at_index(idx);
}

static void play_playlist_track_at_index(int idx) {
    if (idx < 0 || idx >= playlist_count) return;
    playlist_index = idx;
    
    const char *path = playlist_paths[playlist_index];
    audio_play_file(path);
    
    sqlite3 *db;
    char *title = NULL;
    char *artist = NULL;
    char *album = NULL;
    char *cover = NULL;
    char *genre = NULL;
    int year = 0;
    double duration = 0.0;
    int sr = 0, ch = 0;
    
    if (sqlite3_open(db_file, &db) == SQLITE_OK) {
        sqlite3_stmt *stmt;
        const char *sql = "SELECT title, artist, album, cover_path, duration, samplerate, channels, genre, year FROM tracks WHERE path = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char *t = sqlite3_column_text(stmt, 0);
                const unsigned char *a = sqlite3_column_text(stmt, 1);
                const unsigned char *al = sqlite3_column_text(stmt, 2);
                const unsigned char *c = sqlite3_column_text(stmt, 3);
                if (t) title = strdup((const char*)t);
                if (a) artist = strdup((const char*)a);
                if (al) album = strdup((const char*)al);
                if (c) cover = strdup((const char*)c);
                duration = sqlite3_column_double(stmt, 4);
                sr = sqlite3_column_int(stmt, 5);
                ch = sqlite3_column_int(stmt, 6);
                const unsigned char *g = sqlite3_column_text(stmt, 7);
                if (g) genre = strdup((const char*)g);
                year = sqlite3_column_int(stmt, 8);
            }
            sqlite3_finalize(stmt);
        }
        sqlite3_close(db);
    }
    
    gtk_label_set_text(GTK_LABEL(lbl_track_title), title ? title : "Unknown");
    gtk_label_set_text(GTK_LABEL(lbl_track_artist), artist ? artist : "Unknown");
    gtk_label_set_text(GTK_LABEL(lbl_track_album), album ? album : "Unknown");
    gtk_label_set_text(GTK_LABEL(lbl_track_genre), genre ? genre : "Unknown");
    
    gtk_label_set_text(GTK_LABEL(lbl_hdr_title), title ? title : "Unknown");
    gtk_label_set_text(GTK_LABEL(lbl_hdr_artist), artist ? artist : "Unknown");
    gtk_label_set_text(GTK_LABEL(lbl_hdr_album), album ? album : "Unknown");
    
    char yr_str[32];
    if (year > 0) snprintf(yr_str, sizeof(yr_str), "%d", year);
    else strcpy(yr_str, "Unknown");
    gtk_label_set_text(GTK_LABEL(lbl_track_year), yr_str);
    
    double file_size_bytes = 0.0;
    struct stat st;
    if (stat(path, &st) == 0) {
        file_size_bytes = (double)st.st_size;
    }
    
    double actual_mb = file_size_bytes / (1024.0 * 1024.0);
    int actual_bitrate = 0;
    if (duration > 0.0) {
        actual_bitrate = (int)round((file_size_bytes * 8.0) / (duration * 1000.0));
    }

    const char *ext = strrchr(path, '.');
    char format_name[16] = "FLAC";
    if (ext) {
        strncpy(format_name, ext + 1, sizeof(format_name) - 1);
        format_name[sizeof(format_name) - 1] = '\0';
        for (int i = 0; format_name[i]; i++) {
            if (format_name[i] >= 'a' && format_name[i] <= 'z') {
                format_name[i] -= 32;
            }
        }
    }

    char sr_str[64];
    snprintf(sr_str, sizeof(sr_str), "%.1f kHz (%s)", sr / 1000.0f, format_name);
    gtk_label_set_text(GTK_LABEL(lbl_track_sr), sr_str);
    
    char format_str[64];
    snprintf(format_str, sizeof(format_str), "%.1f kHz %s", sr / 1000.0f, format_name);
    gtk_label_set_text(GTK_LABEL(lbl_hdr_format), format_str);
    
    char ch_str[32];
    snprintf(ch_str, sizeof(ch_str), "%d Ch", ch);
    gtk_label_set_text(GTK_LABEL(lbl_track_ch), ch_str);
    
    char bit_str[64];
    snprintf(bit_str, sizeof(bit_str), "%d kbps", actual_bitrate);
    gtk_label_set_text(GTK_LABEL(lbl_track_bitrate), bit_str);
    gtk_label_set_text(GTK_LABEL(lbl_hdr_bitrate), bit_str);
    
    char size_str[64];
    snprintf(size_str, sizeof(size_str), "%.2f MB", actual_mb);
    gtk_label_set_text(GTK_LABEL(lbl_track_size), size_str);
    
    gtk_label_set_text(GTK_LABEL(lbl_track_path), path);
    gtk_label_set_text(GTK_LABEL(lbl_hdr_path), path);
    
    if (cover && strlen(cover) > 0 && access(cover, F_OK) == 0) {
        GdkTexture *tex = gdk_texture_new_from_filename(cover, NULL);
        if (tex) {
            gtk_picture_set_paintable(GTK_PICTURE(img_cover_left), GDK_PAINTABLE(tex));
            g_object_unref(tex);
        }
    } else {
        GdkTexture *placeholder = create_flat_color_texture(200, 200, 0x161618ff);
        gtk_picture_set_paintable(GTK_PICTURE(img_cover_left), GDK_PAINTABLE(placeholder));
        g_object_unref(placeholder);
    }
    
    gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-pause-symbolic");
    if (btn_mid_play_pause) gtk_button_set_icon_name(GTK_BUTTON(btn_mid_play_pause), "media-playback-pause-symbolic");
    refresh_playlist_queue_ui();
    
    if (title) free(title);
    if (artist) free(artist);
    if (album) free(album);
    if (cover) free(cover);
    if (genre) free(genre);
}



// ==========================================
// Genres list box Populating & Filtering
// ==========================================
static void on_genre_row_selected(GtkListBox *listbox, GtkListBoxRow *row, gpointer user_data) {
    (void)listbox;
    (void)user_data;
    if (!row) return;
    
    int idx = gtk_list_box_row_get_index(row);
    if (idx == 0) {
        if (current_selected_genre) {
            free(current_selected_genre);
            current_selected_genre = NULL;
        }
    } else {
        int count = 0;
        char **genres = db_get_genres(&count);
        if (idx - 1 < count) {
            if (current_selected_genre) free(current_selected_genre);
            current_selected_genre = strdup(genres[idx - 1]);
        }
        db_free_genres(genres, count);
    }
    ui_refresh_all_tracks();
}

static void refresh_genres_list(void) {
    if (!list_genres) return;
    
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(list_genres)) != NULL) {
        gtk_list_box_remove(GTK_LIST_BOX(list_genres), child);
    }
    
    GtkWidget *lbl_all = gtk_label_new("<All Genres>");
    gtk_widget_set_halign(lbl_all, GTK_ALIGN_START);
    gtk_list_box_append(GTK_LIST_BOX(list_genres), lbl_all);
    
    int count = 0;
    char **genres = db_get_genres(&count);
    for (int i = 0; i < count; i++) {
        GtkWidget *lbl_g = gtk_label_new(genres[i]);
        gtk_widget_set_halign(lbl_g, GTK_ALIGN_START);
        gtk_list_box_append(GTK_LIST_BOX(list_genres), lbl_g);
    }
    db_free_genres(genres, count);
}

static void on_all_tracks_row_activated(GtkListBox *listbox, GtkListBoxRow *row, gpointer user_data) {
    (void)listbox;
    (void)user_data;
    int idx = gtk_list_box_row_get_index(row) - 1; // Subtract 1 because index 0 is the header row
    if (idx < 0 || idx >= all_tracks_table_count) return;
    
    // Copy all tracks paths to the playlist queue
    if (playlist_paths) {
        for (int i = 0; i < playlist_count; i++) free(playlist_paths[i]);
        free(playlist_paths);
    }
    playlist_count = all_tracks_table_count;
    playlist_paths = malloc(playlist_count * sizeof(char*));
    for (int i = 0; i < playlist_count; i++) {
        playlist_paths[i] = strdup(all_tracks_table_paths[i]);
    }
    
    play_playlist_track_at_index(idx);
}

static void on_album_child_activated(GtkFlowBox *box, GtkFlowBoxChild *child, gpointer user_data) {
    (void)box; (void)user_data;
    const char *album_name = (const char *)g_object_get_data(G_OBJECT(child), "album_name");
    if (album_name && txt_lib_search) {
        gtk_editable_set_text(GTK_EDITABLE(txt_lib_search), album_name);
        if (lib_sub_stack) gtk_stack_set_visible_child_name(GTK_STACK(lib_sub_stack), "library");
    }
}

static void ui_refresh_albums(void) {
    if (!flow_albums) return;
    
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(flow_albums)) != NULL) {
        gtk_flow_box_remove(GTK_FLOW_BOX(flow_albums), child);
    }
    
    int count = 0;
    Album *albums = db_get_unique_albums(&count);
    
    for (int i = 0; i < count; i++) {
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_size_request(box, 160, 180);
        
        GtkWidget *img = NULL;
        if (albums[i].cover_path && strlen(albums[i].cover_path) > 0 && access(albums[i].cover_path, F_OK) == 0) {
            GdkTexture *tex = gdk_texture_new_from_filename(albums[i].cover_path, NULL);
            if (tex) {
                img = gtk_image_new_from_paintable(GDK_PAINTABLE(tex));
                g_object_unref(tex);
            }
        }
        if (!img) {
            GdkTexture *ph = create_flat_color_texture(160, 160, 0x161618ff);
            img = gtk_image_new_from_paintable(GDK_PAINTABLE(ph));
            g_object_unref(ph);
        }
        gtk_image_set_pixel_size(GTK_IMAGE(img), 160);
        gtk_widget_add_css_class(img, "album-cover");
        gtk_box_append(GTK_BOX(box), img);
        
        GtkWidget *lbl = gtk_label_new(albums[i].name);
        gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
        gtk_label_set_wrap_mode(GTK_LABEL(lbl), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_lines(GTK_LABEL(lbl), 2);
        gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);
        gtk_widget_add_css_class(lbl, "eq-label-small");
        gtk_box_append(GTK_BOX(box), lbl);
        
        GtkWidget *flow_child = gtk_flow_box_child_new();
        gtk_flow_box_child_set_child(GTK_FLOW_BOX_CHILD(flow_child), box);
        g_object_set_data_full(G_OBJECT(flow_child), "album_name", strdup(albums[i].name), free);
        
        gtk_flow_box_insert(GTK_FLOW_BOX(flow_albums), flow_child, -1);
    }
    
    db_free_unique_albums(albums, count);
}

static void ui_refresh_all_tracks(void) {
    if (!list_all_tracks) return;
    
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(list_all_tracks)) != NULL) {
        gtk_list_box_remove(GTK_LIST_BOX(list_all_tracks), child);
    }
    
    if (all_tracks_table_paths) {
        for (int i = 0; i < all_tracks_table_count; i++) free(all_tracks_table_paths[i]);
        free(all_tracks_table_paths);
        all_tracks_table_paths = NULL;
        all_tracks_table_count = 0;
    }
    
    const char *query = NULL;
    if (txt_lib_search) {
        query = gtk_editable_get_text(GTK_EDITABLE(txt_lib_search));
    }
    
    int track_count = 0;
    Track *tracks = db_search_tracks(query, current_selected_genre, &track_count);
    
    all_tracks_table_count = track_count;
    if (track_count > 0) {
        all_tracks_table_paths = malloc(track_count * sizeof(char*));
    }
    
    // 1. Header Row
    GtkWidget *hdr_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_add_css_class(hdr_box, "playlist-row-box");
    gtk_widget_add_css_class(hdr_box, "table-header-row");
    
    GtkWidget *lbl_h_num = gtk_label_new("#");
    gtk_widget_set_size_request(lbl_h_num, 30, -1);
    gtk_widget_set_halign(lbl_h_num, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_num);
    
    GtkWidget *lbl_h_title = gtk_label_new("Title");
    gtk_widget_set_halign(lbl_h_title, GTK_ALIGN_START);
    gtk_widget_set_size_request(lbl_h_title, 150, -1);
    gtk_widget_set_hexpand(lbl_h_title, TRUE);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_title);
    
    GtkWidget *lbl_h_art = gtk_label_new("Artist");
    gtk_widget_set_halign(lbl_h_art, GTK_ALIGN_START);
    gtk_widget_set_size_request(lbl_h_art, 120, -1);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_art);
    
    GtkWidget *lbl_h_alb = gtk_label_new("Album");
    gtk_widget_set_halign(lbl_h_alb, GTK_ALIGN_START);
    gtk_widget_set_size_request(lbl_h_alb, 120, -1);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_alb);
    
    GtkWidget *lbl_h_year = gtk_label_new("Year");
    gtk_widget_set_halign(lbl_h_year, GTK_ALIGN_END);
    gtk_widget_set_size_request(lbl_h_year, 40, -1);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_year);
    
    GtkWidget *lbl_h_dur = gtk_label_new("Dur.");
    gtk_widget_set_halign(lbl_h_dur, GTK_ALIGN_END);
    gtk_widget_set_size_request(lbl_h_dur, 50, -1);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_dur);
    
    GtkWidget *lbl_h_bit = gtk_label_new("Bitrate");
    gtk_widget_set_halign(lbl_h_bit, GTK_ALIGN_END);
    gtk_widget_set_size_request(lbl_h_bit, 70, -1);
    gtk_box_append(GTK_BOX(hdr_box), lbl_h_bit);
    
    GtkWidget *hdr_row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(hdr_row), FALSE);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(hdr_row), hdr_box);
    gtk_list_box_append(GTK_LIST_BOX(list_all_tracks), hdr_row);
    
    // 2. Data Rows
    for (int i = 0; i < track_count; i++) {
        all_tracks_table_paths[i] = strdup(tracks[i].path);
        
        GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
        gtk_widget_add_css_class(row_box, "playlist-row-box");
        
        char num_str[16];
        snprintf(num_str, sizeof(num_str), "%d", i + 1);
        GtkWidget *lbl_num = gtk_label_new(num_str);
        gtk_widget_set_size_request(lbl_num, 30, -1);
        gtk_widget_set_halign(lbl_num, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(row_box), lbl_num);
        
        GtkWidget *lbl_title = gtk_label_new(tracks[i].title);
        gtk_widget_set_halign(lbl_title, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(lbl_title), PANGO_ELLIPSIZE_END);
        gtk_widget_set_size_request(lbl_title, 150, -1);
        gtk_widget_set_hexpand(lbl_title, TRUE);
        gtk_box_append(GTK_BOX(row_box), lbl_title);
        
        GtkWidget *lbl_art = gtk_label_new(tracks[i].artist);
        gtk_widget_set_halign(lbl_art, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(lbl_art), PANGO_ELLIPSIZE_END);
        gtk_widget_set_size_request(lbl_art, 120, -1);
        gtk_box_append(GTK_BOX(row_box), lbl_art);
        
        GtkWidget *lbl_alb = gtk_label_new(tracks[i].album);
        gtk_widget_set_halign(lbl_alb, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(lbl_alb), PANGO_ELLIPSIZE_END);
        gtk_widget_set_size_request(lbl_alb, 120, -1);
        gtk_box_append(GTK_BOX(row_box), lbl_alb);
        
        char year_str[16] = "";
        if (tracks[i].year > 0) snprintf(year_str, sizeof(year_str), "%d", tracks[i].year);
        GtkWidget *lbl_year = gtk_label_new(year_str);
        gtk_widget_set_halign(lbl_year, GTK_ALIGN_END);
        gtk_widget_set_size_request(lbl_year, 40, -1);
        gtk_box_append(GTK_BOX(row_box), lbl_year);
        
        int mins = (int)tracks[i].duration / 60;
        int secs = (int)tracks[i].duration % 60;
        char dur_str[16];
        snprintf(dur_str, sizeof(dur_str), "%d:%02d", mins, secs);
        GtkWidget *lbl_dur = gtk_label_new(dur_str);
        gtk_widget_set_halign(lbl_dur, GTK_ALIGN_END);
        gtk_widget_set_size_request(lbl_dur, 50, -1);
        gtk_box_append(GTK_BOX(row_box), lbl_dur);
        
        double file_size_bytes = 0.0;
        struct stat st;
        if (stat(tracks[i].path, &st) == 0) {
            file_size_bytes = (double)st.st_size;
        }
        int actual_bitrate = 0;
        if (tracks[i].duration > 0.0) {
            actual_bitrate = (int)round((file_size_bytes * 8.0) / (tracks[i].duration * 1000.0));
        }
        char bit_str[32];
        snprintf(bit_str, sizeof(bit_str), "%d kbps", actual_bitrate);
        GtkWidget *lbl_bit = gtk_label_new(bit_str);
        gtk_widget_set_halign(lbl_bit, GTK_ALIGN_END);
        gtk_widget_set_size_request(lbl_bit, 70, -1);
        gtk_box_append(GTK_BOX(row_box), lbl_bit);
        
        GtkWidget *row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
        gtk_list_box_append(GTK_LIST_BOX(list_all_tracks), row);
    }
    
    if (tracks) {
        db_free_tracks(tracks, track_count);
    }
}

static void on_lib_search_changed(GtkEditable *editable, gpointer user_data) {
    (void)editable;
    (void)user_data;
    ui_refresh_all_tracks();
}

static void ui_refresh_all(void) {
    ui_refresh_all_tracks();
    ui_refresh_albums();
    refresh_folders_list();
    refresh_genres_list();
}

// ==========================================
// Control Buttons Event Handlers
// ==========================================
static void on_btn_play_pause_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    PlaybackState state;
    audio_get_status(&state, NULL, NULL, NULL, NULL, NULL);
    
    if (state == PLAYBACK_PLAYING) {
        audio_pause();
        gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-start-symbolic");
        if (btn_mid_play_pause) gtk_button_set_icon_name(GTK_BUTTON(btn_mid_play_pause), "media-playback-start-symbolic");
    } else if (state == PLAYBACK_PAUSED) {
        audio_resume();
        gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-pause-symbolic");
    if (btn_mid_play_pause) gtk_button_set_icon_name(GTK_BUTTON(btn_mid_play_pause), "media-playback-pause-symbolic");
    } else {
        if (playlist_count > 0) {
            play_playlist_track_at_index(0);
        }
    }
}

static void on_btn_stop_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    audio_stop();
    gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-start-symbolic");
        if (btn_mid_play_pause) gtk_button_set_icon_name(GTK_BUTTON(btn_mid_play_pause), "media-playback-start-symbolic");
    gtk_range_set_value(GTK_RANGE(scale_seek), 0.0);
    gtk_label_set_text(GTK_LABEL(lbl_time_elapsed), "0:00");
}

static void on_btn_prev_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    if (playlist_count == 0) return;
    int idx = playlist_index - 1;
    if (idx < 0) idx = playlist_count - 1;
    play_playlist_track_at_index(idx);
}

static void on_btn_next_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    if (playlist_count == 0) return;
    
    int idx;
    if (playlist_shuffle) {
        idx = rand() % playlist_count;
    } else {
        idx = playlist_index + 1;
        if (idx >= playlist_count) idx = 0;
    }
    play_playlist_track_at_index(idx);
}

static void on_seek_scale_change(GtkRange *range, gpointer user_data) {
    (void)user_data;
    double val = gtk_range_get_value(range);
    audio_seek(val);
}

static void on_volume_scale_change(GtkRange *range, gpointer user_data) {
    (void)user_data;
    double val = gtk_range_get_value(range);
    audio_set_volume((float)val);
}

static void on_btn_shuffle_toggled(GtkToggleButton *btn, gpointer user_data) {
    (void)user_data;
    playlist_shuffle = gtk_toggle_button_get_active(btn);
}

static void on_btn_loop_toggled(GtkToggleButton *btn, gpointer user_data) {
    (void)user_data;
    playlist_loop = gtk_toggle_button_get_active(btn);
}

// ==========================================
// Equalizer Slider Event Handlers
// ==========================================
static void save_current_eq_state(void) {
    if (!eq_preamp_slider) return;
    EQPreset p;
    p.name = "last_state";
    p.preamp = (float)gtk_range_get_value(GTK_RANGE(eq_preamp_slider));
    for (int i = 0; i < 10; i++) {
        if (eq_sliders[i]) {
            p.bands[i] = (float)gtk_range_get_value(GTK_RANGE(eq_sliders[i]));
        } else {
            p.bands[i] = 0.0f;
        }
    }
    db_save_preset(&p);
}

static void on_eq_slider_changed(GtkRange *range, gpointer user_data) {
    int band = GPOINTER_TO_INT(user_data);
    double val = gtk_range_get_value(range);
    audio_set_band_gain(&g_audio.eq, band, (float)val);
    
    char val_str[16];
    snprintf(val_str, sizeof(val_str), "%d", (int)round(val));
    if (eq_band_val_labels[band]) {
        gtk_label_set_text(GTK_LABEL(eq_band_val_labels[band]), val_str);
    }
    save_current_eq_state();
}

static void on_eq_preamp_changed(GtkRange *range, gpointer user_data) {
    (void)user_data;
    double val = gtk_range_get_value(range);
    audio_set_preamp(&g_audio.eq, (float)val);
    
    char val_str[16];
    snprintf(val_str, sizeof(val_str), "%d", (int)round(val));
    if (eq_preamp_val_label) {
        gtk_label_set_text(GTK_LABEL(eq_preamp_val_label), val_str);
    }
    save_current_eq_state();
}

static void on_eq_enable_toggled(GtkCheckButton *btn, gpointer user_data) {
    (void)user_data;
    pthread_mutex_lock(&g_audio.mutex);
    g_audio.eq_enabled = gtk_check_button_get_active(btn);
    pthread_mutex_unlock(&g_audio.mutex);
}

static void on_preset_row_activated(GtkListBox *listbox, GtkListBoxRow *row, gpointer user_data) {
    (void)listbox;
    GtkWidget *popover = GTK_WIDGET(user_data);
    int idx = gtk_list_box_row_get_index(row);
    
    const char *preset_name = "flat";
    if (idx == 1) preset_name = "rock";
    else if (idx == 2) preset_name = "pop";
    else if (idx == 3) preset_name = "jazz";
    else if (idx == 4) preset_name = "custom";
    
    EQPreset p;
    if (db_load_preset(preset_name, &p)) {
        for (int i = 0; i < 10; i++) {
            audio_set_band_gain(&g_audio.eq, i, p.bands[i]);
            if (eq_sliders[i]) {
                g_signal_handlers_block_by_func(eq_sliders[i], on_eq_slider_changed, GINT_TO_POINTER(i));
                gtk_range_set_value(GTK_RANGE(eq_sliders[i]), p.bands[i]);
                g_signal_handlers_unblock_by_func(eq_sliders[i], on_eq_slider_changed, GINT_TO_POINTER(i));
            }
            if (eq_band_val_labels[i]) {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d", (int)round(p.bands[i]));
                gtk_label_set_text(GTK_LABEL(eq_band_val_labels[i]), val_str);
            }
        }

        audio_set_preamp(&g_audio.eq, p.preamp);
        if (eq_preamp_slider) {
            g_signal_handlers_block_by_func(eq_preamp_slider, on_eq_preamp_changed, NULL);
            gtk_range_set_value(GTK_RANGE(eq_preamp_slider), p.preamp);
            g_signal_handlers_unblock_by_func(eq_preamp_slider, on_eq_preamp_changed, NULL);
        }
        if (eq_preamp_val_label) {
            char val_str[16];
            snprintf(val_str, sizeof(val_str), "%d", (int)round(p.preamp));
            gtk_label_set_text(GTK_LABEL(eq_preamp_val_label), val_str);
        }

        free(p.name);
    }
    
    save_current_eq_state();
    gtk_popover_popdown(GTK_POPOVER(popover));
}

static void on_btn_eq_reset_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    
    if (eq_preamp_slider) {
        g_signal_handlers_block_by_func(eq_preamp_slider, on_eq_preamp_changed, NULL);
        gtk_range_set_value(GTK_RANGE(eq_preamp_slider), 0.0);
        g_signal_handlers_unblock_by_func(eq_preamp_slider, on_eq_preamp_changed, NULL);
    }
    audio_set_preamp(&g_audio.eq, 0.0);
    if (eq_preamp_val_label) {
        gtk_label_set_text(GTK_LABEL(eq_preamp_val_label), "0");
    }

    for (int i = 0; i < 10; i++) {
        if (eq_sliders[i]) {
            g_signal_handlers_block_by_func(eq_sliders[i], on_eq_slider_changed, GINT_TO_POINTER(i));
            gtk_range_set_value(GTK_RANGE(eq_sliders[i]), 0.0);
            g_signal_handlers_unblock_by_func(eq_sliders[i], on_eq_slider_changed, GINT_TO_POINTER(i));
        }
        audio_set_band_gain(&g_audio.eq, i, 0.0);
        if (eq_band_val_labels[i]) {
            gtk_label_set_text(GTK_LABEL(eq_band_val_labels[i]), "0");
        }
    }
    save_current_eq_state();
}

static void on_btn_save_preset_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    if (!eq_preamp_slider) return;
    EQPreset p;
    p.name = "custom";
    p.preamp = (float)gtk_range_get_value(GTK_RANGE(eq_preamp_slider));
    for (int i = 0; i < 10; i++) {
        p.bands[i] = (float)gtk_range_get_value(GTK_RANGE(eq_sliders[i]));
    }
    db_save_preset(&p);
}

static GtkWidget *eq_window = NULL;

static void on_eq_window_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    GtkWidget **win_ptr = (GtkWidget **)user_data;
    *win_ptr = NULL;
}

static void on_btn_eq_toggle_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    if (eq_window) {
        gtk_window_present(GTK_WINDOW(eq_window));
        return;
    }
    
    eq_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(eq_window), "Equalizer");
    gtk_window_set_default_size(GTK_WINDOW(eq_window), 580, 260);
    gtk_window_set_resizable(GTK_WINDOW(eq_window), FALSE);
    gtk_window_set_transient_for(GTK_WINDOW(eq_window), GTK_WINDOW(main_window));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(eq_window), TRUE);
    g_signal_connect(eq_window, "destroy", G_CALLBACK(on_eq_window_destroy), &eq_window);
    
    GtkWidget *box_eq_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_padding(box_eq_page, 20, 20, 20, 20);
    gtk_window_set_child(GTK_WINDOW(eq_window), box_eq_page);
    
    // Top Options Row (Enable, Reset, Presets)
    GtkWidget *box_eq_options = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_set_halign(box_eq_options, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box_eq_page), box_eq_options);

    GtkWidget *chk_eq_enable = gtk_check_button_new_with_label("Enable");
    pthread_mutex_lock(&g_audio.mutex);
    bool eq_active = g_audio.eq_enabled;
    pthread_mutex_unlock(&g_audio.mutex);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(chk_eq_enable), eq_active);
    g_signal_connect(chk_eq_enable, "toggled", G_CALLBACK(on_eq_enable_toggled), NULL);
    gtk_box_append(GTK_BOX(box_eq_options), chk_eq_enable);

    GtkWidget *btn_eq_reset = gtk_button_new_with_label("Reset to Zero");
    g_signal_connect(btn_eq_reset, "clicked", G_CALLBACK(on_btn_eq_reset_clicked), NULL);
    gtk_box_append(GTK_BOX(box_eq_options), btn_eq_reset);

    // Presets Button & Popover
    GtkWidget *btn_presets = gtk_button_new_with_label("Presets...");
    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, btn_presets);
    
    GtkWidget *pop_list = gtk_list_box_new();
    g_signal_connect(pop_list, "row-activated", G_CALLBACK(on_preset_row_activated), popover);
    
    GtkWidget *row_flat = gtk_label_new("Flat");
    gtk_widget_set_padding(row_flat, 8, 8, 4, 4);
    gtk_list_box_append(GTK_LIST_BOX(pop_list), row_flat);
    
    GtkWidget *row_rock = gtk_label_new("Rock");
    gtk_widget_set_padding(row_rock, 8, 8, 4, 4);
    gtk_list_box_append(GTK_LIST_BOX(pop_list), row_rock);
    
    GtkWidget *row_pop = gtk_label_new("Pop");
    gtk_widget_set_padding(row_pop, 8, 8, 4, 4);
    gtk_list_box_append(GTK_LIST_BOX(pop_list), row_pop);
    
    GtkWidget *row_jazz = gtk_label_new("Jazz");
    gtk_widget_set_padding(row_jazz, 8, 8, 4, 4);
    gtk_list_box_append(GTK_LIST_BOX(pop_list), row_jazz);
    
    GtkWidget *row_custom = gtk_label_new("User Custom");
    gtk_widget_set_padding(row_custom, 8, 8, 4, 4);
    gtk_list_box_append(GTK_LIST_BOX(pop_list), row_custom);
    
    gtk_popover_set_child(GTK_POPOVER(popover), pop_list);
    g_signal_connect_swapped(btn_presets, "clicked", G_CALLBACK(gtk_popover_popup), popover);
    gtk_box_append(GTK_BOX(box_eq_options), btn_presets);

    GtkWidget *btn_save_preset = gtk_button_new_with_label("Save Preset");
    g_signal_connect(btn_save_preset, "clicked", G_CALLBACK(on_btn_save_preset_clicked), NULL);
    gtk_box_append(GTK_BOX(box_eq_options), btn_save_preset);

    GtkWidget *box_sliders_panel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_add_css_class(box_sliders_panel, "eq-panel");
    gtk_widget_set_vexpand(box_sliders_panel, TRUE);
    gtk_box_append(GTK_BOX(box_eq_page), box_sliders_panel);

    // Preamp Slider Column
    GtkWidget *box_preamp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(box_preamp, "eq-slider-box");
    
    GtkWidget *lbl_pa = gtk_label_new("P\nr\ne\na\nm\np");
    gtk_widget_add_css_class(lbl_pa, "eq-slider-label");
    gtk_box_append(GTK_BOX(box_preamp), lbl_pa);
    
    eq_preamp_slider = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, -12.0, 12.0, 0.5);
    gtk_scale_set_draw_value(GTK_SCALE(eq_preamp_slider), FALSE);
    gtk_range_set_inverted(GTK_RANGE(eq_preamp_slider), TRUE);
    gtk_widget_set_vexpand(eq_preamp_slider, TRUE);
    gtk_range_set_value(GTK_RANGE(eq_preamp_slider), g_audio.eq.preamp);
    gtk_range_set_value(GTK_RANGE(eq_preamp_slider), 0.0);
    g_signal_connect(eq_preamp_slider, "value-changed", G_CALLBACK(on_eq_preamp_changed), NULL);
    gtk_box_append(GTK_BOX(box_preamp), eq_preamp_slider);
    
    char pa_val_str[16];
    snprintf(pa_val_str, sizeof(pa_val_str), "%d", (int)round(g_audio.eq.preamp));
    eq_preamp_val_label = gtk_label_new(pa_val_str);
    gtk_widget_add_css_class(eq_preamp_val_label, "eq-val-label");
    gtk_box_append(GTK_BOX(box_preamp), eq_preamp_val_label);
    
    gtk_box_append(GTK_BOX(box_sliders_panel), box_preamp);

    // Band Sliders
    const char *band_labels[10] = {"31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"};
    for (int i = 0; i < 10; i++) {
        GtkWidget *box_slider = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_add_css_class(box_slider, "eq-slider-box");
        
        GtkWidget *lbl_b = gtk_label_new(band_labels[i]);
        gtk_widget_add_css_class(lbl_b, "eq-slider-label");
        gtk_box_append(GTK_BOX(box_slider), lbl_b);
        
        eq_sliders[i] = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, -12.0, 12.0, 0.5);
        gtk_scale_set_draw_value(GTK_SCALE(eq_sliders[i]), FALSE);
        gtk_range_set_inverted(GTK_RANGE(eq_sliders[i]), TRUE);
        gtk_widget_set_vexpand(eq_sliders[i], TRUE);
        gtk_range_set_value(GTK_RANGE(eq_sliders[i]), g_audio.eq.gains[i]);
        g_signal_connect(eq_sliders[i], "value-changed", G_CALLBACK(on_eq_slider_changed), GINT_TO_POINTER(i));
        gtk_box_append(GTK_BOX(box_slider), eq_sliders[i]);
        
        char b_val_str[16];
        snprintf(b_val_str, sizeof(b_val_str), "%d", (int)round(g_audio.eq.gains[i]));
        eq_band_val_labels[i] = gtk_label_new(b_val_str);
        gtk_widget_add_css_class(eq_band_val_labels[i], "eq-val-label");
        gtk_box_append(GTK_BOX(box_slider), eq_band_val_labels[i]);
        
        gtk_box_append(GTK_BOX(box_sliders_panel), box_slider);
    }
    
    gtk_window_present(GTK_WINDOW(eq_window));
}

// ==========================================
// Downloader Event Handlers
// ==========================================
static void on_btn_search_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    const char *query = gtk_editable_get_text(GTK_EDITABLE(entry_stream_url));
    if (!query || strlen(query) == 0) return;
    
    gtk_label_set_text(GTK_LABEL(lbl_download_status), "Searching YouTube...");
    
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(box_search_results)) != NULL) {
        gtk_box_remove(GTK_BOX(box_search_results), child);
    }
    
    void on_search_results_ready(SearchResult *results, int count, void *user_data);
    downloader_search_async(query, on_search_results_ready, NULL);
}

static void on_download_progress_update(float progress, const char *status, bool finished, bool success, void *user_data) {
    (void)user_data;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_download), progress);
    gtk_label_set_text(GTK_LABEL(lbl_download_status), status);
    
    if (finished) {
        active_download_id[0] = '\0';
        if (success) {
            trigger_scan(music_dir);
        }
    }
}

static void on_btn_download_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    const char *video_id = (const char *)g_object_get_data(G_OBJECT(btn), "video_id");
    
    // Update active download ID immediately
    gtk_label_set_text(GTK_LABEL(lbl_download_status), "Starting download...");
    if (strlen(active_download_id) > 0) {
        gtk_label_set_text(GTK_LABEL(lbl_download_status), "Another download in progress!");
        return;
    }
    
    strncpy(active_download_id, video_id, sizeof(active_download_id) - 1);
    downloader_download_track_async(video_id, music_dir, on_download_progress_update, NULL);
}

void on_search_results_ready(SearchResult *results, int count, void *user_data) {
    (void)user_data;
    if (count <= 0) {
        gtk_label_set_text(GTK_LABEL(lbl_download_status), "No results found.");
        return;
    }
    
    gtk_label_set_text(GTK_LABEL(lbl_download_status), "Results loaded.");
    
    for (int i = 0; i < count; i++) {
        SearchResult r = results[i];
        
        GtkWidget *item_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
        gtk_widget_set_padding(item_box, 8, 8, 8, 8);
        
        GtkWidget *img;
        if (strlen(r.thumbnail_url) > 0 && access(r.thumbnail_url, F_OK) == 0) {
            GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(r.thumbnail_url, 80, 45, TRUE, NULL);
            if (pb) {
                img = gtk_image_new_from_pixbuf(pb);
                g_object_unref(pb);
            } else {
                img = gtk_image_new_from_icon_name("video-x-generic");
            }
        } else {
            img = gtk_image_new_from_icon_name("video-x-generic");
        }
        gtk_widget_set_size_request(img, 80, 45);
        gtk_box_append(GTK_BOX(item_box), img);
        
        GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_hexpand(text_box, TRUE);
        
        GtkWidget *lbl_t = gtk_label_new(r.title);
        gtk_widget_set_halign(lbl_t, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(lbl_t), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(lbl_t, "album-title");
        gtk_box_append(GTK_BOX(text_box), lbl_t);
        
        GtkWidget *lbl_a = gtk_label_new(r.artist);
        gtk_widget_set_halign(lbl_a, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(lbl_a), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(lbl_a, "album-artist");
        gtk_box_append(GTK_BOX(text_box), lbl_a);
        
        gtk_box_append(GTK_BOX(item_box), text_box);
        
        GtkWidget *lbl_dur = gtk_label_new(r.duration_str);
        gtk_box_append(GTK_BOX(item_box), lbl_dur);
        
        GtkWidget *btn_stream = gtk_button_new_with_label("Stream");
        gtk_widget_add_css_class(btn_stream, "suggested-action");
        g_object_set_data_full(G_OBJECT(btn_stream), "video_id", strdup(r.id), free);
        g_signal_connect(btn_stream, "clicked", G_CALLBACK(on_btn_stream_result_clicked), NULL);
        gtk_box_append(GTK_BOX(item_box), btn_stream);

        GtkWidget *btn = gtk_button_new_with_label("Download FLAC");
        gtk_widget_add_css_class(btn, "suggested-action");
        g_object_set_data_full(G_OBJECT(btn), "video_id", strdup(r.id), free);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_btn_download_clicked), NULL);
        gtk_box_append(GTK_BOX(item_box), btn);
        
        gtk_box_append(GTK_BOX(box_search_results), item_box);
    }
}

// ==========================================
// Folders Management Operations
// ==========================================
static void on_btn_add_folder_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    open_folder_chooser(main_window);
}

static void on_btn_scan_manual_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkEntry *entry = GTK_ENTRY(user_data);
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (path && strlen(path) > 0) {
        db_add_folder(path);
        trigger_scan(path);
        gtk_editable_set_text(GTK_EDITABLE(entry), "");
        refresh_folders_list();
    }
}

static void on_btn_remove_folder_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    const char *path = (const char *)user_data;
    db_remove_folder(path);
    refresh_folders_list();
}

static void refresh_folders_list(void) {
    if (!box_folders_list) return;
    
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(box_folders_list)) != NULL) {
        gtk_box_remove(GTK_BOX(box_folders_list), child);
    }
    
    int count = 0;
    char **folders = db_get_folders(&count);
    
    for (int i = 0; i < count; i++) {
        char *path = folders[i];
        
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        
        GtkWidget *lbl = gtk_label_new(path);
        gtk_widget_set_hexpand(lbl, TRUE);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(row), lbl);
        
        GtkWidget *btn = gtk_button_new_from_icon_name("window-close-symbolic");
        g_signal_connect(btn, "clicked", G_CALLBACK(on_btn_remove_folder_clicked), strdup(path));
        gtk_box_append(GTK_BOX(row), btn);
        
        gtk_box_append(GTK_BOX(box_folders_list), row);
    }
    db_free_folders(folders, count);
}




// ==========================================
// Main tick loop: ticks every 30ms (for fluid 33fps animations)
// ==========================================
static gboolean ui_tick_timer(gpointer user_data) {
    (void)user_data;
    
    // Redraw visualizer
    if (drawing_area_visualizer) {
        gtk_widget_queue_draw(GTK_WIDGET(drawing_area_visualizer));
    }
    PlaybackState state;
    double current, total;
    int sr, ch;
    float vol;
    
    audio_get_status(&state, &current, &total, &sr, &ch, &vol);
    
    pthread_mutex_lock(&g_audio.mutex);
    const char *current_path = g_audio.path;
    pthread_mutex_unlock(&g_audio.mutex);
    
    static char last_playing_path[1024] = {0};
    if (current_path && strcmp(last_playing_path, current_path) != 0) {
        strncpy(last_playing_path, current_path, sizeof(last_playing_path) - 1);
        
        if (list_all_tracks && all_tracks_table_paths) {
            for (int i = 0; i < all_tracks_table_count; i++) {
                if (all_tracks_table_paths[i] && strcmp(all_tracks_table_paths[i], current_path) == 0) {
                    GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_all_tracks), i + 1);
                    if (row) gtk_list_box_select_row(GTK_LIST_BOX(list_all_tracks), row);
                    break;
                }
            }
        }
        
        if (list_playlist && playlist_paths) {
            for (int i = 0; i < playlist_count; i++) {
                if (playlist_paths[i] && strcmp(playlist_paths[i], current_path) == 0) {
                    GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_playlist), i + 1);
                    if (row) gtk_list_box_select_row(GTK_LIST_BOX(list_playlist), row);
                    break;
                }
            }
        }
    }
    
    if (state == PLAYBACK_PLAYING) {
        g_signal_handlers_block_by_func(scale_seek, on_seek_scale_change, NULL);
        gtk_range_set_range(GTK_RANGE(scale_seek), 0.0, total);
        gtk_range_set_value(GTK_RANGE(scale_seek), current);
        g_signal_handlers_unblock_by_func(scale_seek, on_seek_scale_change, NULL);
        
        int cur_mins = (int)current / 60;
        int cur_secs = (int)current % 60;
        int tot_mins = (int)total / 60;
        int tot_secs = (int)total % 60;
        
        char elapsed_str[16], total_str[16];
        snprintf(elapsed_str, sizeof(elapsed_str), "%d:%02d", cur_mins, cur_secs);
        snprintf(total_str, sizeof(total_str), "%d:%02d", tot_mins, tot_secs);
        
        gtk_label_set_text(GTK_LABEL(lbl_time_elapsed), elapsed_str);
        gtk_label_set_text(GTK_LABEL(lbl_time_total), total_str);
        
    } else if (state == PLAYBACK_STOPPED) {
        gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-start-symbolic");
        if (btn_mid_play_pause) gtk_button_set_icon_name(GTK_BUTTON(btn_mid_play_pause), "media-playback-start-symbolic");
        
        bool ended = false;
        pthread_mutex_lock(&g_audio.mutex);
        ended = g_audio.track_ended;
        g_audio.track_ended = false;
        pthread_mutex_unlock(&g_audio.mutex);
        
        if (ended && playlist_count > 0) {
            if (playlist_loop) {
                play_playlist_track_at_index(playlist_index);
            } else {
                int next_idx;
                if (playlist_shuffle) {
                    next_idx = rand() % playlist_count;
                } else {
                    next_idx = playlist_index + 1;
                    if (next_idx >= playlist_count) next_idx = -1;
                }
                if (next_idx != -1) {
                    play_playlist_track_at_index(next_idx);
                } else {
                    audio_stop();
                    gtk_range_set_value(GTK_RANGE(scale_seek), 0.0);
                    gtk_label_set_text(GTK_LABEL(lbl_time_elapsed), "0:00");
                }
            }
        }
    }
    return TRUE;
}

// Menu Bar Handlers
static void on_menu_file_add_folder(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    open_folder_chooser(main_window);
}

static void on_menu_library_scan(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    int count = 0;
    char **folders = db_get_folders(&count);
    for (int i = 0; i < count; i++) {
        trigger_scan(folders[i]);
    }
    db_free_folders(folders, count);
}

static void on_menu_help_about(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_CLOSE,
                                               "AIMP / Audacious Player Clone v1.3\nDesigned for Linux.");
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}


static void on_btn_eq_flat_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    (void)user_data;
    if (eq_preamp_slider) {
        gtk_range_set_value(GTK_RANGE(eq_preamp_slider), 0.0);
    }
    for (int i = 0; i < 10; i++) {
        if (eq_sliders[i]) {
            gtk_range_set_value(GTK_RANGE(eq_sliders[i]), 0.0);
        }
    }
}

// ==========================================
// Key event handler
// ==========================================
static gboolean on_window_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)controller; (void)keycode; (void)state; (void)user_data;
    
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(main_window));
    if (focus && (GTK_IS_EDITABLE(focus) || GTK_IS_ENTRY(focus) || GTK_IS_TEXT(focus) || GTK_IS_SEARCH_ENTRY(focus))) {
        return FALSE; // Do not intercept keys if text is being edited
    }
    
    if (keyval == GDK_KEY_space) {
        on_btn_play_pause_clicked(NULL, NULL);
        return TRUE;
    }
    
    if (keyval == GDK_KEY_Left) {
        pthread_mutex_lock(&g_audio.mutex);
        double target = g_audio.current_time - 5.0;
        if (target < 0) target = 0;
        pthread_mutex_unlock(&g_audio.mutex);
        audio_seek(target);
        return TRUE;
    }
    
    if (keyval == GDK_KEY_Right) {
        pthread_mutex_lock(&g_audio.mutex);
        double target = g_audio.current_time + 5.0;
        if (target > g_audio.duration) target = g_audio.duration;
        pthread_mutex_unlock(&g_audio.mutex);
        audio_seek(target);
        return TRUE;
    }

    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (focus && GTK_IS_LIST_BOX(focus)) {
            GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(focus));
            if (row) {
                g_signal_emit_by_name(focus, "row-activated", row);
                return TRUE;
            }
        }
    }
    
    return FALSE;
}

// ==========================================
// Main UI setup — exact AIMP/Audacious v1.3 layout
// ==========================================
void ui_init(GtkApplication *app) {
    
    const char *config_dir = g_get_user_config_dir();
    char app_dir[1024];
    snprintf(app_dir, sizeof(app_dir), "%s/sonora", config_dir);
    mkdir(app_dir, 0755);
    
    snprintf(covers_dir, sizeof(covers_dir), "%s/covers", app_dir);
    snprintf(db_file, sizeof(db_file), "%s/sonora.db", app_dir);
    
    mkdir(covers_dir, 0755);
    db_init(db_file);
    srand(time(NULL));

    // Force Dark Theme
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);
    g_object_set(gtk_settings_get_default(), "gtk-primary-button-warps-slider", TRUE, NULL);

    // Main Window
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "AIMP / Audacious Player v1.3");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1060, 640);
    
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_window_key_pressed), NULL);
    gtk_widget_add_controller(main_window, key_ctrl);

    GtkWidget *box_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(main_window), box_main);

    // ── TOP BAR (Playback + Meta) ──────────────────────────────────────────
    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(top_bar, "top-bar");
    gtk_box_append(GTK_BOX(box_main), top_bar);

    // 1. Seek Bar (Full Width Top)
    GtkWidget *box_seek = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign(box_seek, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(box_seek, TRUE);
    gtk_widget_add_css_class(box_seek, "seek-bar-box");
    gtk_box_append(GTK_BOX(top_bar), box_seek);

    lbl_time_elapsed = gtk_label_new("0:00");
    gtk_widget_set_valign(lbl_time_elapsed, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(lbl_time_elapsed, "prop-val");
    gtk_box_append(GTK_BOX(box_seek), lbl_time_elapsed);

    scale_seek = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
    gtk_widget_set_valign(scale_seek, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(scale_seek, TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(scale_seek), FALSE);
    gtk_widget_add_css_class(scale_seek, "seek-scale-thin");
    g_signal_connect(scale_seek, "value-changed", G_CALLBACK(on_seek_scale_change), NULL);
    gtk_box_append(GTK_BOX(box_seek), scale_seek);

    lbl_time_total = gtk_label_new("0:00");
    gtk_widget_set_valign(lbl_time_total, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(lbl_time_total, "prop-val");
    gtk_box_append(GTK_BOX(box_seek), lbl_time_total);

    // 2. Controls and Metadata in one line
    GtkWidget *box_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(box_controls, 6);
    gtk_widget_set_margin_bottom(box_controls, 4);
    gtk_box_append(GTK_BOX(top_bar), box_controls);

    // Left: playback buttons
    GtkWidget *box_pb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_valign(box_pb, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box_controls), box_pb);

    btn_play_pause = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    g_signal_connect(btn_play_pause, "clicked", G_CALLBACK(on_btn_play_pause_clicked), NULL);
    gtk_box_append(GTK_BOX(box_pb), btn_play_pause);
    
    GtkWidget *btn_stop2 = gtk_button_new_from_icon_name("media-playback-stop-symbolic");
    g_signal_connect(btn_stop2, "clicked", G_CALLBACK(on_btn_stop_clicked), NULL);
    gtk_box_append(GTK_BOX(box_pb), btn_stop2);

    GtkWidget *btn_prev2 = gtk_button_new_from_icon_name("media-skip-backward-symbolic");
    g_signal_connect(btn_prev2, "clicked", G_CALLBACK(on_btn_prev_clicked), NULL);
    gtk_box_append(GTK_BOX(box_pb), btn_prev2);

    GtkWidget *btn_next2 = gtk_button_new_from_icon_name("media-skip-forward-symbolic");
    g_signal_connect(btn_next2, "clicked", G_CALLBACK(on_btn_next_clicked), NULL);
    gtk_box_append(GTK_BOX(box_pb), btn_next2);

    GtkWidget *btn_shuffle2 = gtk_toggle_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(btn_shuffle2), "media-playlist-shuffle-symbolic");
    g_signal_connect(btn_shuffle2, "toggled", G_CALLBACK(on_btn_shuffle_toggled), NULL);
    gtk_box_append(GTK_BOX(box_pb), btn_shuffle2);

    GtkWidget *btn_loop2 = gtk_toggle_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(btn_loop2), "media-playlist-repeat-symbolic");
    g_signal_connect(btn_loop2, "toggled", G_CALLBACK(on_btn_loop_toggled), NULL);
    gtk_box_append(GTK_BOX(box_pb), btn_loop2);

    // Meta HBox (Right aligned)
    GtkWidget *box_meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_valign(box_meta, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(box_meta, TRUE);
    gtk_widget_set_halign(box_meta, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box_controls), box_meta);

    // Title
    lbl_hdr_title = gtk_label_new("Unknown Title");
    gtk_widget_add_css_class(lbl_hdr_title, "prop-val");
    gtk_label_set_ellipsize(GTK_LABEL(lbl_hdr_title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request(lbl_hdr_title, 140, -1);
    gtk_box_append(GTK_BOX(box_meta), lbl_hdr_title);

    // Artist
    GtkWidget *lbl_a = gtk_label_new("•");
    gtk_widget_add_css_class(lbl_a, "prop-label");
    gtk_box_append(GTK_BOX(box_meta), lbl_a);

    lbl_hdr_artist = gtk_label_new("Unknown Artist");
    gtk_widget_add_css_class(lbl_hdr_artist, "prop-val");
    gtk_label_set_ellipsize(GTK_LABEL(lbl_hdr_artist), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request(lbl_hdr_artist, 140, -1);
    gtk_box_append(GTK_BOX(box_meta), lbl_hdr_artist);

    // Album
    GtkWidget *lbl_al = gtk_label_new("•");
    gtk_widget_add_css_class(lbl_al, "prop-label");
    gtk_box_append(GTK_BOX(box_meta), lbl_al);

    lbl_hdr_album = gtk_label_new("Unknown Album");
    gtk_widget_add_css_class(lbl_hdr_album, "prop-val");
    gtk_label_set_ellipsize(GTK_LABEL(lbl_hdr_album), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request(lbl_hdr_album, 140, -1);
    gtk_box_append(GTK_BOX(box_meta), lbl_hdr_album);

    // Format & Bitrate combined visually
    GtkWidget *lbl_f = gtk_label_new("•");
    gtk_widget_add_css_class(lbl_f, "prop-label");
    gtk_box_append(GTK_BOX(box_meta), lbl_f);

    lbl_hdr_format = gtk_label_new("44.1 kHz FLAC");
    gtk_widget_add_css_class(lbl_hdr_format, "prop-val");
    gtk_box_append(GTK_BOX(box_meta), lbl_hdr_format);

    lbl_hdr_bitrate = gtk_label_new("0 kbps");
    gtk_widget_add_css_class(lbl_hdr_bitrate, "prop-val");
    gtk_box_append(GTK_BOX(box_meta), lbl_hdr_bitrate);

    // Hidden path label so the app doesn't crash on updates
    lbl_hdr_path = gtk_label_new("No File Loaded");
    gtk_widget_set_visible(lbl_hdr_path, FALSE);
    gtk_box_append(GTK_BOX(box_meta), lbl_hdr_path);

    // Theme Dropdown
    const char *theme_names[] = {
        "Dark Orange", "Dark Green", "Dark Blue",
        "Light Orange", "Light Green", "Light Blue",
        NULL
    };
    GtkWidget *theme_dropdown = gtk_drop_down_new_from_strings(theme_names);
    gtk_widget_set_valign(theme_dropdown, GTK_ALIGN_CENTER);
    
    char *saved_theme = db_get_setting("theme_index");
    int saved_idx = 0;
    if (saved_theme) {
        saved_idx = atoi(saved_theme);
        free(saved_theme);
        if (saved_idx < 0 || saved_idx >= 6) {
            saved_idx = 0;
        }
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(theme_dropdown), saved_idx);
    
    gtk_widget_set_focus_on_click(theme_dropdown, FALSE);
    g_signal_connect(theme_dropdown, "notify::selected", G_CALLBACK(on_theme_selected), NULL);
    gtk_box_append(GTK_BOX(box_controls), theme_dropdown);

    // ── MAIN AREA ──────────────────────────────────────────────────────────
    GtkWidget *box_body = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(box_body), 380);
    gtk_paned_set_wide_handle(GTK_PANED(box_body), TRUE);
    gtk_widget_set_vexpand(box_body, TRUE);
    gtk_box_append(GTK_BOX(box_main), box_body);

    // ── MIDDLE COLUMN ──────────────────────────────────────────────────────
    GtkWidget *box_mid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); // Added spacing
    gtk_widget_add_css_class(box_mid, "middle-col");
    gtk_widget_set_size_request(box_mid, 220, -1);
    gtk_widget_set_hexpand(box_mid, FALSE);
    // Removed valign center to allow background to fill vertical space
    gtk_paned_set_start_child(GTK_PANED(box_body), box_mid);

    GtkWidget *spacer_top = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer_top, TRUE);
    gtk_box_append(GTK_BOX(box_mid), spacer_top);

    GdkTexture *ph = create_flat_color_texture(350, 350, 0x000000ff);
    img_cover_left = gtk_picture_new_for_paintable(GDK_PAINTABLE(ph));
    gtk_picture_set_can_shrink(GTK_PICTURE(img_cover_left), TRUE);
    g_object_unref(ph);
    gtk_widget_add_css_class(img_cover_left, "album-cover");
    gtk_widget_set_halign(img_cover_left, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box_mid), img_cover_left);

    GtkWidget *grid_props = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_props), 2);
    gtk_grid_set_column_spacing(GTK_GRID(grid_props), 4);
    gtk_widget_add_css_class(grid_props, "track-props-grid");
    gtk_box_append(GTK_BOX(box_mid), grid_props);

    static const char *plabels[] = {
        "TITLE:", "ARTIST:", "ALBUM:", "GENRE:", "YEAR:",
        "FORMAT:", "BITRATE:", "SIZE:", "PATH:"
    };
    lbl_track_title   = gtk_label_new("Unknown");
    lbl_track_artist  = gtk_label_new("Unknown");
    lbl_track_album   = gtk_label_new("Unknown");
    lbl_track_genre   = gtk_label_new("Unknown");
    lbl_track_year    = gtk_label_new("Unknown");
    lbl_track_sr      = gtk_label_new("Unknown");
    lbl_track_bitrate = gtk_label_new("Unknown");
    lbl_track_size    = gtk_label_new("Unknown");
    lbl_track_path    = gtk_label_new("Unknown");
    // dummy ch for compat
    lbl_track_ch = gtk_label_new("");

    GtkWidget *pvals[] = {
        lbl_track_title, lbl_track_artist, lbl_track_album,
        lbl_track_genre, lbl_track_year,   lbl_track_sr,
        lbl_track_bitrate, lbl_track_size, lbl_track_path
    };
    for (int r = 0; r < 9; r++) {
        GtkWidget *ltag = gtk_label_new(plabels[r]);
        gtk_widget_add_css_class(ltag, "prop-label");
        gtk_widget_set_halign(ltag, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid_props), ltag, 0, r, 1, 1);

        gtk_widget_add_css_class(pvals[r], "prop-val");
        gtk_widget_set_halign(pvals[r], GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(pvals[r]), PANGO_ELLIPSIZE_END);
        gtk_widget_set_hexpand(pvals[r], TRUE);
        gtk_grid_attach(GTK_GRID(grid_props), pvals[r], 1, r, 1, 1);
    }

    GtkWidget *spacer_mid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer_mid, TRUE);
    gtk_box_append(GTK_BOX(box_mid), spacer_mid);

    // Removed box_seek and box_mid_pb from here

    // ── RIGHT COLUMN (Playlist & EQ) ───────────────────────────────────────
    GtkWidget *box_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(box_right, TRUE);
    gtk_widget_add_css_class(box_right, "right-col");
    gtk_paned_set_end_child(GTK_PANED(box_body), box_right);

    lib_sub_stack = gtk_stack_new();
    gtk_widget_set_hexpand(lib_sub_stack, TRUE);
    gtk_widget_set_vexpand(lib_sub_stack, TRUE);
    gtk_stack_set_transition_type(GTK_STACK(lib_sub_stack), GTK_STACK_TRANSITION_TYPE_NONE);

    GtkWidget *switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher), GTK_STACK(lib_sub_stack));
    gtk_widget_set_halign(switcher, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(switcher, TRUE);
    gtk_widget_add_css_class(switcher, "tabs");
    gtk_box_append(GTK_BOX(box_right), switcher);
    gtk_box_append(GTK_BOX(box_right), lib_sub_stack);

    // Tab 1: Playlist
    GtkWidget *box_pl_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    // Header Row
    GtkWidget *pl_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(pl_header, "playlist-header");
    gtk_box_append(GTK_BOX(box_pl_tab), pl_header);
    
    GtkWidget *h_id = gtk_label_new("ID"); gtk_widget_set_size_request(h_id, 30, -1); gtk_widget_set_halign(h_id, GTK_ALIGN_START); gtk_box_append(GTK_BOX(pl_header), h_id);
    GtkWidget *h_arr = gtk_label_new("▲"); gtk_widget_set_size_request(h_arr, 20, -1); gtk_widget_set_halign(h_arr, GTK_ALIGN_CENTER); gtk_box_append(GTK_BOX(pl_header), h_arr);
    GtkWidget *h_trk = gtk_label_new("Track"); gtk_widget_set_size_request(h_trk, 200, -1); gtk_widget_set_halign(h_trk, GTK_ALIGN_START); gtk_box_append(GTK_BOX(pl_header), h_trk);
    GtkWidget *h_art = gtk_label_new("Artist"); gtk_widget_set_size_request(h_art, 150, -1); gtk_widget_set_halign(h_art, GTK_ALIGN_START); gtk_box_append(GTK_BOX(pl_header), h_art);
    GtkWidget *h_alb = gtk_label_new("Album"); gtk_widget_set_size_request(h_alb, 200, -1); gtk_widget_set_halign(h_alb, GTK_ALIGN_START); gtk_box_append(GTK_BOX(pl_header), h_alb);
    GtkWidget *h_sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_widget_set_hexpand(h_sp, TRUE); gtk_box_append(GTK_BOX(pl_header), h_sp);
    GtkWidget *h_tim = gtk_label_new("Time"); gtk_widget_set_size_request(h_tim, 50, -1); gtk_widget_set_halign(h_tim, GTK_ALIGN_END); gtk_box_append(GTK_BOX(pl_header), h_tim);
    GtkWidget *h_bit = gtk_label_new("Bitrate"); gtk_widget_set_size_request(h_bit, 70, -1); gtk_widget_set_halign(h_bit, GTK_ALIGN_END); gtk_box_append(GTK_BOX(pl_header), h_bit);

    GtkWidget *scroll_pl = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll_pl, TRUE);
    list_playlist = gtk_list_box_new();
    gtk_widget_add_css_class(list_playlist, "playlist-list");
    g_signal_connect(list_playlist, "row-activated", G_CALLBACK(on_playlist_row_activated), NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_pl), list_playlist);
    gtk_box_append(GTK_BOX(box_pl_tab), scroll_pl);
    gtk_stack_add_titled(GTK_STACK(lib_sub_stack), box_pl_tab, "playlist", "Active Playlist");

    GtkWidget *scroll_albums = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll_albums, TRUE);
    flow_albums = gtk_flow_box_new();
    gtk_flow_box_set_valign(GTK_FLOW_BOX(flow_albums), GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow_albums), 10);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow_albums), GTK_SELECTION_SINGLE);
    g_signal_connect(flow_albums, "child-activated", G_CALLBACK(on_album_child_activated), NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_albums), flow_albums);
    gtk_stack_add_titled(GTK_STACK(lib_sub_stack), scroll_albums, "albums", "Albums");


    // Tab 2: Library (stubbed to same style for now)
    GtkWidget *box_lib_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    // add search to lib
    GtkWidget *box_lib_search = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start(box_lib_search, 4);
    gtk_widget_set_margin_end(box_lib_search, 4);
    gtk_widget_set_margin_top(box_lib_search, 2);
    gtk_widget_set_margin_bottom(box_lib_search, 2);
    gtk_box_append(GTK_BOX(box_lib_tab), box_lib_search);

    txt_lib_search = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(txt_lib_search), "Search database...");
    gtk_widget_set_hexpand(txt_lib_search, TRUE);
    g_signal_connect(txt_lib_search, "changed", G_CALLBACK(on_lib_search_changed), NULL);
    gtk_box_append(GTK_BOX(box_lib_search), txt_lib_search);
    
    GtkWidget *btn_lib_rescan = gtk_button_new_with_label("Rescan Library");
    g_signal_connect(btn_lib_rescan, "clicked", G_CALLBACK(on_btn_lib_rescan_clicked), NULL);
    gtk_box_append(GTK_BOX(box_lib_search), btn_lib_rescan);
    
    // Header Row
    GtkWidget *lib_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(lib_header, "playlist-header");
    gtk_box_append(GTK_BOX(box_lib_tab), lib_header);
    
    GtkWidget *lh_id = gtk_label_new("ID"); gtk_widget_set_size_request(lh_id, 30, -1); gtk_widget_set_halign(lh_id, GTK_ALIGN_START); gtk_box_append(GTK_BOX(lib_header), lh_id);
    GtkWidget *lh_arr = gtk_label_new("▲"); gtk_widget_set_size_request(lh_arr, 20, -1); gtk_widget_set_halign(lh_arr, GTK_ALIGN_CENTER); gtk_box_append(GTK_BOX(lib_header), lh_arr);
    GtkWidget *lh_trk = gtk_label_new("Track"); gtk_widget_set_size_request(lh_trk, 200, -1); gtk_widget_set_halign(lh_trk, GTK_ALIGN_START); gtk_box_append(GTK_BOX(lib_header), lh_trk);
    GtkWidget *lh_art = gtk_label_new("Artist"); gtk_widget_set_size_request(lh_art, 150, -1); gtk_widget_set_halign(lh_art, GTK_ALIGN_START); gtk_box_append(GTK_BOX(lib_header), lh_art);
    GtkWidget *lh_alb = gtk_label_new("Album"); gtk_widget_set_size_request(lh_alb, 200, -1); gtk_widget_set_halign(lh_alb, GTK_ALIGN_START); gtk_box_append(GTK_BOX(lib_header), lh_alb);
    GtkWidget *lh_sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_widget_set_hexpand(lh_sp, TRUE); gtk_box_append(GTK_BOX(lib_header), lh_sp);
    GtkWidget *lh_tim = gtk_label_new("Time"); gtk_widget_set_size_request(lh_tim, 50, -1); gtk_widget_set_halign(lh_tim, GTK_ALIGN_END); gtk_box_append(GTK_BOX(lib_header), lh_tim);
    GtkWidget *lh_bit = gtk_label_new("Bitrate"); gtk_widget_set_size_request(lh_bit, 70, -1); gtk_widget_set_halign(lh_bit, GTK_ALIGN_END); gtk_box_append(GTK_BOX(lib_header), lh_bit);

    GtkWidget *scroll_lib = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll_lib, TRUE);
    list_all_tracks = gtk_list_box_new();
    gtk_widget_add_css_class(list_all_tracks, "playlist-list");
    g_signal_connect(list_all_tracks, "row-activated", G_CALLBACK(on_all_tracks_row_activated), NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_lib), list_all_tracks);
    gtk_box_append(GTK_BOX(box_lib_tab), scroll_lib);
    gtk_stack_add_titled(GTK_STACK(lib_sub_stack), box_lib_tab, "library", "Library");

    // Tab 3: Filesystem View (Folder Management)
    GtkWidget *box_fs_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box_fs_tab, 10);
    gtk_widget_set_margin_end(box_fs_tab, 10);
    gtk_widget_set_margin_top(box_fs_tab, 10);
    gtk_widget_set_margin_bottom(box_fs_tab, 10);

    GtkWidget *lbl_fs_title = gtk_label_new("Library Folders");
    gtk_widget_add_css_class(lbl_fs_title, "title-2");
    gtk_widget_set_halign(lbl_fs_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box_fs_tab), lbl_fs_title);

    // Manual entry + Scan button
    GtkWidget *box_fs_entry = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *entry_fs_path = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_fs_path), "/path/to/music...");
    gtk_widget_set_hexpand(entry_fs_path, TRUE);
    gtk_box_append(GTK_BOX(box_fs_entry), entry_fs_path);

    GtkWidget *btn_fs_scan_manual = gtk_button_new_with_label("Scan Path");
    g_signal_connect(btn_fs_scan_manual, "clicked", G_CALLBACK(on_btn_scan_manual_clicked), entry_fs_path);
    gtk_box_append(GTK_BOX(box_fs_entry), btn_fs_scan_manual);
    gtk_box_append(GTK_BOX(box_fs_tab), box_fs_entry);

    // GTK Chooser button
    GtkWidget *btn_fs_choose = gtk_button_new_with_label("Select Folder via Dialog...");
    g_signal_connect(btn_fs_choose, "clicked", G_CALLBACK(on_btn_add_folder_clicked), NULL);
    gtk_box_append(GTK_BOX(box_fs_tab), btn_fs_choose);

    GtkWidget *btn_fs_rescan_all = gtk_button_new_with_label("Rescan All Folders");
    g_signal_connect(btn_fs_rescan_all, "clicked", G_CALLBACK(on_btn_lib_rescan_clicked), NULL);
    gtk_box_append(GTK_BOX(box_fs_tab), btn_fs_rescan_all);

    // List of folders
    GtkWidget *lbl_fs_list = gtk_label_new("Scanned Folders:");
    gtk_widget_set_halign(lbl_fs_list, GTK_ALIGN_START);
    gtk_widget_set_margin_top(lbl_fs_list, 10);
    gtk_box_append(GTK_BOX(box_fs_tab), lbl_fs_list);

    box_folders_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *scroll_fs = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll_fs, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_fs), box_folders_list);
    gtk_box_append(GTK_BOX(box_fs_tab), scroll_fs);

    gtk_stack_add_titled(GTK_STACK(lib_sub_stack), box_fs_tab, "filesystem", "Filesystem");
    
    // Tab 4: Streams & Downloads
    GtkWidget *box_streams_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box_streams_tab, 10);
    gtk_widget_set_margin_end(box_streams_tab, 10);
    gtk_widget_set_margin_top(box_streams_tab, 10);
    gtk_widget_set_margin_bottom(box_streams_tab, 10);

    GtkWidget *lbl_stream_title = gtk_label_new("Streams & Downloads");
    gtk_widget_add_css_class(lbl_stream_title, "title-2");
    gtk_widget_set_halign(lbl_stream_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box_streams_tab), lbl_stream_title);

    entry_stream_url = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_stream_url), "Enter KHInsider Album / Archive.org URL / YT Music URL...");
    gtk_widget_set_hexpand(entry_stream_url, TRUE);
    gtk_box_append(GTK_BOX(box_streams_tab), entry_stream_url);

    GtkWidget *box_stream_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    btn_dl_khinsider = gtk_button_new_with_label("DL KHInsider FLAC");
    g_signal_connect(btn_dl_khinsider, "clicked", G_CALLBACK(on_btn_dl_khinsider_clicked), NULL);
    gtk_box_append(GTK_BOX(box_stream_btns), btn_dl_khinsider);

    btn_dl_archive = gtk_button_new_with_label("DL Archive.org FLAC");
    g_signal_connect(btn_dl_archive, "clicked", G_CALLBACK(on_btn_dl_archive_clicked), NULL);
    gtk_box_append(GTK_BOX(box_stream_btns), btn_dl_archive);

    GtkWidget *btn_search_yt = gtk_button_new_with_label("Search YouTube");
    gtk_widget_add_css_class(btn_search_yt, "suggested-action");
    g_signal_connect(btn_search_yt, "clicked", G_CALLBACK(on_btn_search_clicked), NULL);
    gtk_box_append(GTK_BOX(box_stream_btns), btn_search_yt);

    gtk_box_append(GTK_BOX(box_streams_tab), box_stream_btns);

    lbl_stream_status = gtk_label_new("Ready.");
    gtk_widget_set_halign(lbl_stream_status, GTK_ALIGN_START);
    gtk_widget_set_margin_top(lbl_stream_status, 10);
    gtk_box_append(GTK_BOX(box_streams_tab), lbl_stream_status);

    prog_stream = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(prog_stream), 0.0);
    gtk_box_append(GTK_BOX(box_streams_tab), prog_stream);

    lbl_download_status = gtk_label_new("");
    gtk_widget_set_halign(lbl_download_status, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box_streams_tab), lbl_download_status);

    GtkWidget *scroll_search = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll_search, TRUE);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll_search), 200);
    box_search_results = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(box_search_results, 10);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_search), box_search_results);
    gtk_box_append(GTK_BOX(box_streams_tab), scroll_search);

    gtk_stack_add_titled(GTK_STACK(lib_sub_stack), box_streams_tab, "streams", "Streams");

    // ── EQ & VISUALIZER STRIP ──────────────────────────────────────────────
    
    GtkWidget *box_dsp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_size_request(box_dsp, -1, 220);
    gtk_widget_set_vexpand_set(box_dsp, TRUE);
    gtk_widget_set_vexpand(box_dsp, FALSE);
    gtk_widget_set_margin_start(box_dsp, 4);
    gtk_widget_set_margin_end(box_dsp, 4);
    gtk_widget_set_margin_bottom(box_dsp, 4);
    gtk_box_append(GTK_BOX(box_right), box_dsp);

    GtkWidget *box_eq = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(box_eq, "eq-panel"); // Move background here
    gtk_widget_set_hexpand(box_eq, FALSE);
    gtk_widget_set_margin_start(box_eq, 2);
    gtk_widget_set_margin_end(box_eq, 2);
    gtk_box_append(GTK_BOX(box_dsp), box_eq);

    // Top Bar
    GtkWidget *box_eq_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(box_eq), box_eq_top);

    GtkWidget *chk_enable = gtk_check_button_new_with_label("Enable");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(chk_enable), TRUE);
    g_signal_connect(chk_enable, "toggled", G_CALLBACK(on_eq_enable_toggled), NULL);
    gtk_box_append(GTK_BOX(box_eq_top), chk_enable);

    GtkWidget *top_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(top_spacer, TRUE);
    gtk_box_append(GTK_BOX(box_eq_top), top_spacer);

    GtkWidget *btn_reset = gtk_button_new_with_label("Reset to Zero");
    gtk_widget_add_css_class(btn_reset, "eq-btn-audacious");
    g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_btn_eq_reset_clicked), NULL);
    gtk_box_append(GTK_BOX(box_eq_top), btn_reset);

    GtkWidget *btn_presets = gtk_button_new_with_label("Presets ...");
    gtk_widget_add_css_class(btn_presets, "eq-btn-audacious");
    g_signal_connect(btn_presets, "clicked", G_CALLBACK(on_btn_eq_flat_clicked), NULL);
    gtk_box_append(GTK_BOX(box_eq_top), btn_presets);

    // Body
    GtkWidget *box_eq_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
    gtk_widget_set_vexpand(box_eq_body, TRUE);
    gtk_box_append(GTK_BOX(box_eq), box_eq_body);

    // Preamp
    GtkWidget *box_pre = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(box_eq_body), box_pre);
    
    GtkWidget *lbl_pre = gtk_label_new("Preamp"); 
    gtk_widget_add_css_class(lbl_pre, "eq-label-rotated"); 
    gtk_box_append(GTK_BOX(box_pre), lbl_pre);
    
    eq_preamp_slider = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, -12.0, 12.0, 0.5);
    gtk_range_set_inverted(GTK_RANGE(eq_preamp_slider), TRUE);
    gtk_widget_set_vexpand(eq_preamp_slider, TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(eq_preamp_slider), FALSE);
    gtk_widget_add_css_class(eq_preamp_slider, "eq-slider-audacious");
    gtk_range_set_value(GTK_RANGE(eq_preamp_slider), 0.0);
    g_signal_connect(eq_preamp_slider, "value-changed", G_CALLBACK(on_eq_preamp_changed), NULL);
    gtk_box_append(GTK_BOX(box_pre), eq_preamp_slider);
    
    eq_preamp_val_label = gtk_label_new("0"); 
    gtk_widget_add_css_class(eq_preamp_val_label, "eq-label-small"); 
    gtk_box_append(GTK_BOX(box_pre), eq_preamp_val_label);

    // Separator
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(sep, 4);
    gtk_widget_set_margin_end(sep, 4);
    gtk_box_append(GTK_BOX(box_eq_body), sep);

    // 10 Bands
    const char *freqs[] = {"31 Hz", "63 Hz", "125 Hz", "250 Hz", "500 Hz", "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"};
    for (int i = 0; i < 10; i++) {
        GtkWidget *box_b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_box_append(GTK_BOX(box_eq_body), box_b);
        
        GtkWidget *lbl_f = gtk_label_new(freqs[i]); 
        gtk_widget_add_css_class(lbl_f, "eq-label-rotated"); 
        gtk_box_append(GTK_BOX(box_b), lbl_f);
        
        eq_sliders[i] = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, -12.0, 12.0, 0.5);
        gtk_range_set_inverted(GTK_RANGE(eq_sliders[i]), TRUE);
        gtk_widget_set_vexpand(eq_sliders[i], TRUE);
        gtk_scale_set_draw_value(GTK_SCALE(eq_sliders[i]), FALSE);
        gtk_widget_add_css_class(eq_sliders[i], "eq-slider-audacious");
        gtk_range_set_value(GTK_RANGE(eq_sliders[i]), 0.0);
        g_signal_connect(eq_sliders[i], "value-changed", G_CALLBACK(on_eq_slider_changed), GINT_TO_POINTER(i));
        gtk_box_append(GTK_BOX(box_b), eq_sliders[i]);
        
        eq_band_val_labels[i] = gtk_label_new("0"); 
        gtk_widget_add_css_class(eq_band_val_labels[i], "eq-label-small"); 
        gtk_box_append(GTK_BOX(box_b), eq_band_val_labels[i]);
    }
    
    GtkWidget *vis_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(vis_panel, "vis-panel");
    gtk_widget_set_size_request(vis_panel, 100, -1);
    gtk_widget_set_hexpand(vis_panel, TRUE);
    gtk_widget_set_vexpand(vis_panel, TRUE);
    
    drawing_area_visualizer = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawing_area_visualizer, TRUE);
    gtk_widget_set_vexpand(drawing_area_visualizer, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area_visualizer), on_visualizer_draw, NULL, NULL);
    
    gtk_box_append(GTK_BOX(vis_panel), drawing_area_visualizer);
    
    GtkWidget *box_vol = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_valign(box_vol, GTK_ALIGN_END);
    gtk_widget_set_halign(box_vol, GTK_ALIGN_END);
    
    GtkWidget *img_vol = gtk_image_new_from_icon_name("audio-volume-high-symbolic");
    gtk_box_append(GTK_BOX(box_vol), img_vol);
    scale_volume = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.05);
    gtk_widget_set_size_request(scale_volume, 80, -1);
    gtk_range_set_value(GTK_RANGE(scale_volume), 1.0);
    gtk_scale_set_draw_value(GTK_SCALE(scale_volume), FALSE);
    gtk_widget_add_css_class(scale_volume, "volume-scale");
    g_signal_connect(scale_volume, "value-changed", G_CALLBACK(on_volume_scale_change), NULL);
    gtk_box_append(GTK_BOX(box_vol), scale_volume);
    
    gtk_box_append(GTK_BOX(vis_panel), box_vol);
    
    gtk_box_append(GTK_BOX(box_dsp), vis_panel);



    // ── Init defaults ─────────────────────────────────────────────────
    EQPreset rock_preset = {"rock", 0.0f, {4.0f, 3.0f, 2.0f, -1.0f, -2.0f, -1.0f, 1.0f, 3.0f, 4.0f, 5.0f}};
    db_save_preset(&rock_preset);

    EQPreset p;
    if (db_load_preset("last_state", &p)) {
        audio_set_preamp(&g_audio.eq, p.preamp);
        if (eq_preamp_slider) {
            g_signal_handlers_block_by_func(eq_preamp_slider, on_eq_preamp_changed, NULL);
            gtk_range_set_value(GTK_RANGE(eq_preamp_slider), p.preamp);
            g_signal_handlers_unblock_by_func(eq_preamp_slider, on_eq_preamp_changed, NULL);
            if (eq_preamp_val_label) {
                char val_str[16];
                snprintf(val_str, sizeof(val_str), "%d", (int)round(p.preamp));
                gtk_label_set_text(GTK_LABEL(eq_preamp_val_label), val_str);
            }
        }
        for (int i = 0; i < 10; i++) {
            audio_set_band_gain(&g_audio.eq, i, p.bands[i]);
            if (eq_sliders[i]) {
                g_signal_handlers_block_by_func(eq_sliders[i], on_eq_slider_changed, GINT_TO_POINTER(i));
                gtk_range_set_value(GTK_RANGE(eq_sliders[i]), p.bands[i]);
                g_signal_handlers_unblock_by_func(eq_sliders[i], on_eq_slider_changed, GINT_TO_POINTER(i));
                if (eq_band_val_labels[i]) {
                    char val_str[16];
                    snprintf(val_str, sizeof(val_str), "%d", (int)round(p.bands[i]));
                    gtk_label_set_text(GTK_LABEL(eq_band_val_labels[i]), val_str);
                }
            }
        }
        free(p.name);
    } else {
        audio_set_preamp(&g_audio.eq, 0.0f);
        for (int i = 0; i < 10; i++) audio_set_band_gain(&g_audio.eq, i, 0.0f);
    }

    db_add_folder(music_dir);
    int fc = 0;
    char **fols = db_get_folders(&fc);
    for (int i = 0; i < fc; i++) trigger_scan(fols[i]);
    db_free_folders(fols, fc);

    ui_refresh_all();
    g_timeout_add(30, ui_tick_timer, NULL);
    g_timeout_add_seconds(60, background_scan_timer, NULL);
    // Initialize the default theme
    // Initialize the theme
    int initial_theme_idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(theme_dropdown));
    apply_theme(initial_theme_idx);

    gtk_window_present(GTK_WINDOW(main_window));
}