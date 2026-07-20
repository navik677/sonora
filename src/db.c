#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static sqlite3 *db = NULL;
static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

static int exec_sql(const char *sql) {
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool db_init(const char *db_path) {
    pthread_mutex_lock(&db_mutex);
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        pthread_mutex_unlock(&db_mutex);
        return false;
    }
    sqlite3_busy_timeout(db, 5000);
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "ALTER TABLE tracks ADD COLUMN genre TEXT;", NULL, NULL, NULL);

    // Create tables
    const char *sql_folders = 
        "CREATE TABLE IF NOT EXISTS folders ("
        "path TEXT PRIMARY KEY"
        ");";
    const char *sql_tracks = 
        "CREATE TABLE IF NOT EXISTS tracks ("
        "path TEXT PRIMARY KEY, "
        "title TEXT, "
        "artist TEXT, "
        "album TEXT, "
        "genre TEXT, "
        "year INTEGER, "
        "duration REAL, "
        "samplerate INTEGER, "
        "channels INTEGER, "
        "cover_path TEXT"
        ");";
    const char *sql_presets = 
        "CREATE TABLE IF NOT EXISTS eq_presets ("
        "name TEXT PRIMARY KEY, "
        "preamp REAL, "
        "band0 REAL, band1 REAL, band2 REAL, band3 REAL, band4 REAL, "
        "band5 REAL, band6 REAL, band7 REAL, band8 REAL, band9 REAL, band10 REAL, band11 REAL, band12 REAL, band13 REAL, band14 REAL"
        ");";

    const char *sql_settings = 
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY, "
        "value TEXT"
        ");";

    exec_sql("ALTER TABLE eq_presets ADD COLUMN band10 REAL;");
    exec_sql("ALTER TABLE eq_presets ADD COLUMN band11 REAL;");
    exec_sql("ALTER TABLE eq_presets ADD COLUMN band12 REAL;");
    exec_sql("ALTER TABLE eq_presets ADD COLUMN band13 REAL;");
    exec_sql("ALTER TABLE eq_presets ADD COLUMN band14 REAL;");

    if (!exec_sql(sql_folders) || !exec_sql(sql_tracks) || !exec_sql(sql_presets) || !exec_sql(sql_settings)) {
        sqlite3_close(db);
        db = NULL;
        pthread_mutex_unlock(&db_mutex);
        return false;
    }

    pthread_mutex_unlock(&db_mutex);
    return true;
}

void db_close(void) {
    pthread_mutex_lock(&db_mutex);
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
    pthread_mutex_unlock(&db_mutex);
}

bool db_add_folder(const char *path) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR IGNORE INTO folders (path) VALUES (?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return false;
    }
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return (rc == SQLITE_DONE || rc == SQLITE_ROW);
}

bool db_remove_folder(const char *path) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM folders WHERE path = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return false;
    }
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return (rc == SQLITE_DONE || rc == SQLITE_ROW);
}

char **db_get_folders(int *count) {
    pthread_mutex_lock(&db_mutex);
    *count = 0;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT path FROM folders;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    int capacity = 10;
    char **list = malloc(capacity * sizeof(char*));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof(char*));
        }
        const unsigned char *p = sqlite3_column_text(stmt, 0);
        list[*count] = strdup(p ? (const char*)p : "");
        (*count)++;
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return list;
}

void db_free_folders(char **folders, int count) {
    if (!folders) return;
    for (int i = 0; i < count; i++) {
        free(folders[i]);
    }
    free(folders);
}

bool db_track_exists(const char *path) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT 1 FROM tracks WHERE path = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return false;
    }
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return exists;
}

void db_cleanup_missing_tracks(void) {
    int count = 0;
    Track *tracks = db_get_tracks(&count);
    if (!tracks) return;
    
    for (int i = 0; i < count; i++) {
        if (access(tracks[i].path, F_OK) != 0) {
            db_remove_track(tracks[i].path);
        }
    }
    db_free_tracks(tracks, count);
}


bool db_add_track(const Track *track) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO tracks (path, title, artist, album, genre, year, duration, samplerate, channels, cover_path) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    int prep_rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (prep_rc != SQLITE_OK) {
        fprintf(stderr, "SQLite prepare error in db_add_track: %d, msg: %s\n", prep_rc, sqlite3_errmsg(db));
        pthread_mutex_unlock(&db_mutex);
        return false;
    }

    sqlite3_bind_text(stmt, 1, track->path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, track->title ? track->title : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, track->artist ? track->artist : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, track->album ? track->album : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, track->genre ? track->genre : "Unknown", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, track->year);
    sqlite3_bind_double(stmt, 7, track->duration);
    sqlite3_bind_int(stmt, 8, track->samplerate);
    sqlite3_bind_int(stmt, 9, track->channels);
    sqlite3_bind_text(stmt, 10, track->cover_path ? track->cover_path : "", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        fprintf(stderr, "SQLite add track error: %d, msg: %s\n", rc, sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return (rc == SQLITE_DONE || rc == SQLITE_ROW);
}

bool db_remove_track(const char *path) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM tracks WHERE path = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return false;
    }
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return (rc == SQLITE_DONE || rc == SQLITE_ROW);
}

static Track *fill_tracks_from_stmt(sqlite3_stmt *stmt, int *count) {
    int capacity = 10;
    Track *list = malloc(capacity * sizeof(Track));
    *count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof(Track));
        }
        Track *t = &list[*count];
        const unsigned char *path = sqlite3_column_text(stmt, 0);
        const unsigned char *title = sqlite3_column_text(stmt, 1);
        const unsigned char *artist = sqlite3_column_text(stmt, 2);
        const unsigned char *album = sqlite3_column_text(stmt, 3);
        const unsigned char *genre = sqlite3_column_text(stmt, 4);
        t->path = strdup(path ? (const char*)path : "");
        t->title = strdup(title ? (const char*)title : "");
        t->artist = strdup(artist ? (const char*)artist : "");
        t->album = strdup(album ? (const char*)album : "");
        t->genre = strdup(genre ? (const char*)genre : "Unknown");
        t->year = sqlite3_column_int(stmt, 5);
        t->duration = sqlite3_column_double(stmt, 6);
        t->samplerate = sqlite3_column_int(stmt, 7);
        t->channels = sqlite3_column_int(stmt, 8);
        const unsigned char *cover = sqlite3_column_text(stmt, 9);
        t->cover_path = strdup(cover ? (const char*)cover : "");
        (*count)++;
    }
    return list;
}

Track *db_get_tracks(int *count) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT path, title, artist, album, genre, year, duration, samplerate, channels, cover_path FROM tracks ORDER BY artist, album, year, path;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *count = 0;
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }
    Track *res = fill_tracks_from_stmt(stmt, count);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return res;
}

Track *db_get_tracks_by_album(const char *album, int *count) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT path, title, artist, album, genre, year, duration, samplerate, channels, cover_path FROM tracks WHERE album = ? ORDER BY path;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *count = 0;
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }
    sqlite3_bind_text(stmt, 1, album, -1, SQLITE_STATIC);
    Track *res = fill_tracks_from_stmt(stmt, count);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return res;
}

Track *db_get_tracks_by_genre(const char *genre, int *count) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT path, title, artist, album, genre, year, duration, samplerate, channels, cover_path FROM tracks WHERE genre = ? ORDER BY path;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *count = 0;
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }
    sqlite3_bind_text(stmt, 1, genre, -1, SQLITE_STATIC);
    Track *res = fill_tracks_from_stmt(stmt, count);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return res;
}

char **db_get_albums(int *count) {
    pthread_mutex_lock(&db_mutex);
    *count = 0;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT DISTINCT album FROM tracks WHERE album != '' ORDER BY album;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    int capacity = 10;
    char **list = malloc(capacity * sizeof(char*));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof(char*));
        }
        const unsigned char *p = sqlite3_column_text(stmt, 0);
        list[*count] = strdup(p ? (const char*)p : "");
        (*count)++;
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return list;
}

char **db_get_albums_by_genre(const char *genre, int *count) {
    pthread_mutex_lock(&db_mutex);
    *count = 0;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT DISTINCT album FROM tracks WHERE genre = ? AND album != '' ORDER BY album;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, genre, -1, SQLITE_STATIC);

    int capacity = 10;
    char **list = malloc(capacity * sizeof(char*));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof(char*));
        }
        const unsigned char *p = sqlite3_column_text(stmt, 0);
        list[*count] = strdup(p ? (const char*)p : "");
        (*count)++;
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return list;
}

char **db_get_genres(int *count) {
    pthread_mutex_lock(&db_mutex);
    *count = 0;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT DISTINCT genre FROM tracks WHERE genre != '' ORDER BY genre;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    int capacity = 10;
    char **list = malloc(capacity * sizeof(char*));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof(char*));
        }
        const unsigned char *p = sqlite3_column_text(stmt, 0);
        list[*count] = strdup(p ? (const char*)p : "");
        (*count)++;
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return list;
}

void db_free_tracks(Track *tracks, int count) {
    if (!tracks) return;
    for (int i = 0; i < count; i++) {
        free(tracks[i].path);
        free(tracks[i].title);
        free(tracks[i].artist);
        free(tracks[i].album);
        free(tracks[i].genre);
        free(tracks[i].cover_path);
    }
    free(tracks);
}

void db_free_albums(char **albums, int count) {
    if (!albums) return;
    for (int i = 0; i < count; i++) {
        free(albums[i]);
    }
    free(albums);
}

void db_free_genres(char **genres, int count) {
    if (!genres) return;
    for (int i = 0; i < count; i++) {
        free(genres[i]);
    }
    free(genres);
}

Album *db_get_unique_albums(int *count) {
    pthread_mutex_lock(&db_mutex);
    Album *albums = NULL;
    int capacity = 0;
    *count = 0;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT artist, album, MAX(cover_path) FROM tracks WHERE album IS NOT NULL AND album != 'Unknown Album' GROUP BY album ORDER BY artist, album;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (*count >= capacity) {
                capacity = capacity == 0 ? 16 : capacity * 2;
                albums = realloc(albums, capacity * sizeof(Album));
            }
            const unsigned char *artist = sqlite3_column_text(stmt, 0);
            const unsigned char *name = sqlite3_column_text(stmt, 1);
            const unsigned char *cover_path = sqlite3_column_text(stmt, 2);
            
            albums[*count].artist = strdup(artist ? (const char*)artist : "Unknown Artist");
            albums[*count].name = strdup(name ? (const char*)name : "Unknown Album");
            albums[*count].cover_path = strdup(cover_path ? (const char*)cover_path : "");
            (*count)++;
        }
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&db_mutex);
    return albums;
}

void db_free_unique_albums(Album *albums, int count) {
    if (!albums) return;
    for (int i = 0; i < count; i++) {
        free(albums[i].name);
        free(albums[i].artist);
        free(albums[i].cover_path);
    }
    free(albums);
}

MissingCover *db_get_missing_covers(int *count) {
    pthread_mutex_lock(&db_mutex);
    *count = 0;
    sqlite3_stmt *stmt;
    // Union: first get regular albums grouped by album
    // Then get generic albums, not grouped (individual tracks)
    const char *sql = 
        "SELECT artist, album, NULL as title, MAX(cover_path) FROM tracks "
        "WHERE album IS NOT NULL AND album NOT IN ('Unknown Album', 'Sonora Downloads', '') "
        "GROUP BY album HAVING MAX(cover_path) = '' "
        "UNION ALL "
        "SELECT artist, album, title, cover_path FROM tracks "
        "WHERE (cover_path IS NULL OR cover_path = '') AND album IN ('Unknown Album', 'Sonora Downloads', '') "
        "ORDER BY artist, album, title;";
        
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    int capacity = 16;
    MissingCover *covers = malloc(capacity * sizeof(MissingCover));
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            covers = realloc(covers, capacity * sizeof(MissingCover));
        }
        
        const unsigned char *artist = sqlite3_column_text(stmt, 0);
        const unsigned char *album = sqlite3_column_text(stmt, 1);
        const unsigned char *title = sqlite3_column_text(stmt, 2);
        
        covers[*count].artist = strdup(artist ? (const char*)artist : "Unknown Artist");
        covers[*count].album = strdup(album ? (const char*)album : "Unknown Album");
        covers[*count].title = title ? strdup((const char*)title) : NULL;
        
        (*count)++;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return covers;
}

void db_free_missing_covers(MissingCover *covers, int count) {
    if (!covers) return;
    for (int i = 0; i < count; i++) {
        if (covers[i].artist) free(covers[i].artist);
        if (covers[i].album) free(covers[i].album);
        if (covers[i].title) free(covers[i].title);
    }
    free(covers);
}

void db_update_cover(const MissingCover *mc, const char *cover_path) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    if (mc->title) {
        sqlite3_prepare_v2(db, "UPDATE tracks SET cover_path = ? WHERE artist = ? AND title = ?", -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, cover_path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, mc->artist, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, mc->title, -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_prepare_v2(db, "UPDATE tracks SET cover_path = ? WHERE album = ?", -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, cover_path, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, mc->album, -1, SQLITE_TRANSIENT);
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
}


bool db_save_preset(const EQPreset *preset) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR REPLACE INTO eq_presets (name, preamp, band0, band1, band2, band3, band4, band5, band6, band7, band8, band9) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return false;
    }

    sqlite3_bind_text(stmt, 1, preset->name, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, preset->preamp);
    for (int i = 0; i < 10; i++) {
        sqlite3_bind_double(stmt, i + 3, preset->bands[i]);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return (rc == SQLITE_DONE || rc == SQLITE_ROW);
}

bool db_load_preset(const char *name, EQPreset *preset) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name, preamp, band0, band1, band2, band3, band4, band5, band6, band7, band8, band9 FROM eq_presets WHERE name = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return false;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char *n = sqlite3_column_text(stmt, 0);
        preset->name = strdup(n ? (const char*)n : "");
        preset->preamp = sqlite3_column_double(stmt, 1);
        for (int i = 0; i < 10; i++) {
            preset->bands[i] = (float)sqlite3_column_double(stmt, i + 2);
        }
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return true;
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return false;
}

char **db_get_preset_names(int *count) {
    pthread_mutex_lock(&db_mutex);
    *count = 0;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name FROM eq_presets ORDER BY name;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    int capacity = 10;
    char **list = malloc(capacity * sizeof(char*));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof(char*));
        }
        const unsigned char *p = sqlite3_column_text(stmt, 0);
        list[*count] = strdup(p ? (const char*)p : "");
        (*count)++;
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return list;
}

void db_free_preset_names(char **names, int count) {
    if (!names) return;
    for (int i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
}

bool db_set_setting(const char *key, const char *value) {
    if (!key || !value) return false;
    pthread_mutex_lock(&db_mutex);
    const char *sql = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return true;
    }
    pthread_mutex_unlock(&db_mutex);
    return false;
}

char *db_get_setting(const char *key) {
    if (!key) return NULL;
    pthread_mutex_lock(&db_mutex);
    const char *sql = "SELECT value FROM settings WHERE key = ?;";
    sqlite3_stmt *stmt;
    char *result = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *val = (const char *)sqlite3_column_text(stmt, 0);
            if (val) result = strdup(val);
        }
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&db_mutex);
    return result;
}

bool db_has_album_cover(const char *album) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT cover_path FROM tracks WHERE album = ? AND cover_path != '' LIMIT 1;";
    bool has_cover = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, album, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            has_cover = true;
        }
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&db_mutex);
    return has_cover;
}

void db_update_track_cover_by_path(const char *path, const char *cover_path) {
    if (!db || !path || !cover_path) return;
    char *err_msg = NULL;
    char *sql = sqlite3_mprintf("UPDATE tracks SET cover_path = '%q' WHERE path = '%q';", cover_path, path);
    sqlite3_exec(db, sql, NULL, 0, &err_msg);
    sqlite3_free(sql);
    if (err_msg) {
        sqlite3_free(err_msg);
    }
}

bool db_update_album_cover(const char *album, const char *cover_path) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE tracks SET cover_path = ? WHERE album = ?;";
    bool success = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, cover_path, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, album, -1, SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE || rc == SQLITE_ROW) {
            success = true;
        }
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&db_mutex);
    return success;
}

char *db_get_track_cover(const char *path) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT cover_path FROM tracks WHERE path = ?;";
    char *cover = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *c = sqlite3_column_text(stmt, 0);
            if (c && *c) cover = strdup((const char*)c);
        }
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&db_mutex);
    return cover;
}

char *db_get_album_cover(const char *album) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT cover_path FROM tracks WHERE album = ? AND cover_path != '' LIMIT 1;";
    char *cover = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, album, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *c = sqlite3_column_text(stmt, 0);
            if (c && *c) cover = strdup((const char*)c);
        }
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&db_mutex);
    return cover;
}

Track *db_search_tracks(const char *query, const char *genre, int *count) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    char sql[1024];
    
    const char *base_sql = "SELECT path, title, artist, album, genre, year, duration, samplerate, channels, cover_path FROM tracks WHERE 1=1";
    strcpy(sql, base_sql);
    
    if (genre && strlen(genre) > 0) {
        strcat(sql, " AND genre = ?");
    }
    if (query && strlen(query) > 0) {
        strcat(sql, " AND (title LIKE ? OR artist LIKE ? OR album LIKE ?)");
    }
    strcat(sql, " ORDER BY artist, album, year, path;");
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *count = 0;
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }
    
    int bind_idx = 1;
    if (genre && strlen(genre) > 0) {
        sqlite3_bind_text(stmt, bind_idx++, genre, -1, SQLITE_STATIC);
    }
    char *like_str = NULL;
    if (query && strlen(query) > 0) {
        like_str = malloc(strlen(query) + 3);
        sprintf(like_str, "%%%s%%", query);
        sqlite3_bind_text(stmt, bind_idx++, like_str, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, bind_idx++, like_str, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, bind_idx++, like_str, -1, SQLITE_TRANSIENT);
    }
    
    Track *res = fill_tracks_from_stmt(stmt, count);
    sqlite3_finalize(stmt);
    if (like_str) free(like_str);
    pthread_mutex_unlock(&db_mutex);
    return res;
}

char **db_search_albums(const char *query, const char *genre, int *count) {
    pthread_mutex_lock(&db_mutex);
    sqlite3_stmt *stmt;
    char sql[1024];
    
    const char *base_sql = "SELECT DISTINCT album FROM tracks WHERE album != ''";
    strcpy(sql, base_sql);
    
    if (genre && strlen(genre) > 0) {
        strcat(sql, " AND genre = ?");
    }
    if (query && strlen(query) > 0) {
        strcat(sql, " AND (title LIKE ? OR artist LIKE ? OR album LIKE ?)");
    }
    strcat(sql, " ORDER BY album;");
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *count = 0;
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }
    
    int bind_idx = 1;
    if (genre && strlen(genre) > 0) {
        sqlite3_bind_text(stmt, bind_idx++, genre, -1, SQLITE_STATIC);
    }
    char *like_str = NULL;
    if (query && strlen(query) > 0) {
        like_str = malloc(strlen(query) + 3);
        sprintf(like_str, "%%%s%%", query);
        sqlite3_bind_text(stmt, bind_idx++, like_str, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, bind_idx++, like_str, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, bind_idx++, like_str, -1, SQLITE_TRANSIENT);
    }
    
    int capacity = 16;
    int local_count = 0;
    char **list = malloc(capacity * sizeof(char*));
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (local_count >= capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof(char*));
        }
        const unsigned char *p = sqlite3_column_text(stmt, 0);
        list[local_count] = strdup(p ? (const char*)p : "");
        local_count++;
    }
    
    sqlite3_finalize(stmt);
    if (like_str) free(like_str);
    pthread_mutex_unlock(&db_mutex);
    
    *count = local_count;
    return list;
}
