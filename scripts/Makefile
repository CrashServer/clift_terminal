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
LDFLAGS = -lncurses -lm -pthread -lssl -lcrypto -lstdc++ $(PIPEWIRE_LIBS) $(EXTRA_LDFLAGS)
TARGET = ../clift
LINK_WRAPPER_OBJ = link_wrapper.o
AUDIO_OBJ = audio_pipewire.o

all: $(TARGET)

$(LINK_WRAPPER_OBJ): $(SRCDIR)/link_wrapper.cpp $(SRCDIR)/link_wrapper.hpp
	$(CXX) $(CXXFLAGS) -c $(SRCDIR)/link_wrapper.cpp -o $(LINK_WRAPPER_OBJ)

$(AUDIO_OBJ): $(SRCDIR)/audio_pipewire.c $(SRCDIR)/audio_pipewire.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/audio_pipewire.c -o $(AUDIO_OBJ)

$(TARGET): $(SRCDIR)/clift_engine.c $(LINK_WRAPPER_OBJ) $(AUDIO_OBJ)
	$(CC) -o $(TARGET) $(SRCDIR)/clift_engine.c $(LINK_WRAPPER_OBJ) $(AUDIO_OBJ) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(LINK_WRAPPER_OBJ) $(AUDIO_OBJ)

.PHONY: all clean