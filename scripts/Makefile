CC = gcc
CXX = g++
SRCDIR = ../src
# Default to fake Link implementation unless overridden
FAKE_LINK_FLAG ?= -DUSE_FAKE_LINK=1

# PipeWire support - enable by default if libpipewire is available
PIPEWIRE_CFLAGS := $(shell pkg-config --cflags libpipewire-0.3 2>/dev/null)
PIPEWIRE_LIBS := $(shell pkg-config --libs libpipewire-0.3 2>/dev/null)
ifneq ($(PIPEWIRE_LIBS),)
    PIPEWIRE_FLAG = -DUSE_PIPEWIRE=1
else
    PIPEWIRE_FLAG =
    PIPEWIRE_CFLAGS =
    PIPEWIRE_LIBS =
endif

CFLAGS = -O2 -Wall -I$(SRCDIR) $(FAKE_LINK_FLAG) $(PIPEWIRE_FLAG) $(PIPEWIRE_CFLAGS) $(EXTRA_CFLAGS)
CXXFLAGS = $(CFLAGS) -std=c++14
LDFLAGS = -lncursesw -lm -pthread -lssl -lcrypto -lstdc++ $(PIPEWIRE_LIBS) $(EXTRA_LDFLAGS)
TARGET = ../clift
LINK_WRAPPER_OBJ = link_wrapper.o
AUDIO_OBJ = audio_pipewire.o

# Installation mode objects
INSTALL_DIRECTOR_OBJ = installation_director.o
INSTALL_SCENES_OBJ = installation_scenes.o
DATA_FEEDS_OBJ = data_feeds.o
SIMULATED_AUDIO_OBJ = simulated_audio.o
CJSON_OBJ = cjson.o

SURFACE_OBJ = surface_scenes.o

INSTALL_OBJS = $(INSTALL_DIRECTOR_OBJ) $(INSTALL_SCENES_OBJ) $(DATA_FEEDS_OBJ) $(SIMULATED_AUDIO_OBJ) $(CJSON_OBJ)

all: $(TARGET)

$(LINK_WRAPPER_OBJ): $(SRCDIR)/link_wrapper.cpp $(SRCDIR)/link_wrapper.hpp
	$(CXX) $(CXXFLAGS) -c $(SRCDIR)/link_wrapper.cpp -o $(LINK_WRAPPER_OBJ)

$(AUDIO_OBJ): $(SRCDIR)/audio_pipewire.c $(SRCDIR)/audio_pipewire.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/audio_pipewire.c -o $(AUDIO_OBJ)

$(INSTALL_DIRECTOR_OBJ): $(SRCDIR)/installation_director.c $(SRCDIR)/installation_director.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/installation_director.c -o $(INSTALL_DIRECTOR_OBJ)

$(INSTALL_SCENES_OBJ): $(SRCDIR)/installation_scenes.c $(SRCDIR)/installation_scenes.h $(SRCDIR)/data_feeds.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/installation_scenes.c -o $(INSTALL_SCENES_OBJ)

$(DATA_FEEDS_OBJ): $(SRCDIR)/data_feeds.c $(SRCDIR)/data_feeds.h $(SRCDIR)/vendor/cJSON.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/data_feeds.c -o $(DATA_FEEDS_OBJ)

$(SIMULATED_AUDIO_OBJ): $(SRCDIR)/simulated_audio.c $(SRCDIR)/simulated_audio.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/simulated_audio.c -o $(SIMULATED_AUDIO_OBJ)

$(CJSON_OBJ): $(SRCDIR)/vendor/cJSON.c $(SRCDIR)/vendor/cJSON.h
	$(CC) $(CFLAGS) -w -c $(SRCDIR)/vendor/cJSON.c -o $(CJSON_OBJ)

$(SURFACE_OBJ): $(SRCDIR)/surface_scenes.c $(SRCDIR)/surface_scenes.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/surface_scenes.c -o $(SURFACE_OBJ)

$(TARGET): $(SRCDIR)/clift_engine.c $(LINK_WRAPPER_OBJ) $(AUDIO_OBJ) $(INSTALL_OBJS) $(SURFACE_OBJ)
	$(CC) -o $(TARGET) $(SRCDIR)/clift_engine.c $(LINK_WRAPPER_OBJ) $(AUDIO_OBJ) $(INSTALL_OBJS) $(SURFACE_OBJ) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(LINK_WRAPPER_OBJ) $(AUDIO_OBJ) $(INSTALL_OBJS) $(SURFACE_OBJ)

.PHONY: all clean
