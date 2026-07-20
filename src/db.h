#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include <stdbool.h>

typedef struct {
    char *path;
    char *title;
    char *artist;
    char *album;
    char *genre;
    int year;
    double duration;
    int samplerate;
    int channels;
    char *cover_path;
} Track;

typedef struct {
    char *name;
    char *artist;
    char *cover_path;
} Album;

typedef struct {
    char *artist;
    char *album;
    char *title;
} MissingCover;

typedef struct {
    char *name;
    float preamp;
    float bands[10];
} EQPreset;

bool db_init(const char *db_path);
void db_close(void);

// Folder operations
bool db_add_folder(const char *path);
bool db_remove_folder(const char *path);
char **db_get_folders(int *count);
void db_free_folders(char **folders, int count);

// Track operations
bool db_track_exists(const char *path);
void db_cleanup_missing_tracks(void);
bool db_add_track(const Track *track);
bool db_remove_track(const char *path);
Track *db_get_tracks(int *count);
Track *db_get_tracks_by_album(const char *album, int *count);
Track *db_get_tracks_by_genre(const char *genre, int *count);
char **db_get_albums(int *count);
char **db_get_albums_by_genre(const char *genre, int *count);
char **db_get_genres(int *count);
Album *db_get_unique_albums(int *count);
MissingCover *db_get_missing_covers(int *count);
void db_update_cover(const MissingCover *mc, const char *cover_path);
void db_free_unique_albums(Album *albums, int count);
void db_free_missing_covers(MissingCover *covers, int count);
void db_free_tracks(Track *tracks, int count);
void db_free_albums(char **albums, int count);
void db_free_genres(char **genres, int count);
bool db_has_album_cover(const char *album);
bool db_update_album_cover(const char *album, const char *cover_path);
char *db_get_track_cover(const char *path);
void db_update_track_cover_by_path(const char *path, const char *cover_path);
char *db_get_album_cover(const char *album);
Track *db_search_tracks(const char *query, const char *genre, int *count);
char **db_search_albums(const char *query, const char *genre, int *count);

// EQ Preset operations
bool db_save_preset(const EQPreset *preset);
bool db_load_preset(const char *name, EQPreset *preset);
char **db_get_preset_names(int *count);
void db_free_preset_names(char **names, int count);

// Settings operations
bool db_set_setting(const char *key, const char *value);
char *db_get_setting(const char *key);

#endif // DB_H
