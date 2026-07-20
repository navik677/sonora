CC=gcc
CFLAGS=-O3 -Wall -Wextra `pkg-config --cflags gtk4 sqlite3 libpulse-simple sndfile libcurl`
LIBS=`pkg-config --libs gtk4 sqlite3 libpulse-simple sndfile libcurl` -lpthread -lm

SRC_DIR=src
OBJ_DIR=obj

SOURCES=$(SRC_DIR)/main.c $(SRC_DIR)/ui.c $(SRC_DIR)/audio.c $(SRC_DIR)/eq.c $(SRC_DIR)/db.c $(SRC_DIR)/downloader.c $(SRC_DIR)/fft.c
OBJECTS=$(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES)) $(OBJ_DIR)/resources.o

TARGET=sonora

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/resources.o: $(SRC_DIR)/sonora.gresource.xml $(SRC_DIR)/style.css | $(OBJ_DIR)
	glib-compile-resources --sourcedir=$(SRC_DIR) --target=$(OBJ_DIR)/resources.c --generate-source $(SRC_DIR)/sonora.gresource.xml
	$(CC) $(CFLAGS) -c -o $@ $(OBJ_DIR)/resources.c

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
