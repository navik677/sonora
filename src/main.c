#include "audio.h"
#include "db.h"
#include "ui.h"
#include <gtk/gtk.h>
#include <curl/curl.h>

int main(int argc, char **argv) {
    // Allow GTK to respect gtk-application-prefer-dark-theme by removing AppImage wrappers override
    unsetenv("GTK_THEME");

    // Initialize cURL for cover downloads
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Initialize Audio engine (playback thread & PulseAudio client)
    if (!audio_init()) {
        fprintf(stderr, "Failed to initialize audio engine.\n");
        curl_global_cleanup();
        return 1;
    }

    // Initialize GTK Application
    GtkApplication *app = gtk_application_new("com.sonoraplayer.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(ui_init), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    // Cleanup resources
    audio_destroy();
    db_close();
    curl_global_cleanup();

    return status;
}
