CC = gcc
CXX = g++
SRCDIR = ../src
# Default to fake Link implementation unless overridden
FAKE_LINK_FLAG ?= -DUSE_FAKE_LINK=1
CFLAGS = -O2 -Wall -I$(SRCDIR) $(FAKE_LINK_FLAG) $(EXTRA_CFLAGS)
CXXFLAGS = $(CFLAGS) -std=c++14
LDFLAGS = -lncurses -lm -pthread -lssl -lcrypto -lstdc++ $(EXTRA_LDFLAGS)
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