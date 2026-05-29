# ──────────────────────────────────────────────────────────────────
# Makefile for detector.c — Object Detection Summary (Darknet YOLOv3)
# ──────────────────────────────────────────────────────────────────

CC       = gcc
CFLAGS   = -Wall -Ofast
DARKNET  = ../darknet
INCLUDES = -I$(DARKNET)/include -I$(DARKNET)/src

# Link statically against libdarknet.a to avoid LD_LIBRARY_PATH issues
LIBS     = $(DARKNET)/libdarknet.a -lm -lpthread

TARGET   = detector

all: $(TARGET)

$(TARGET): detector.c
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
