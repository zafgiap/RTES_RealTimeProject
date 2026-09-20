# Raspberry Pi Zero / Zero W uses the ARM1176JZF-S ARMv6 processor.
CROSS_COMPILE ?= armv6-rpi-linux-gnueabihf-
SYSROOT       ?= /home/zafeiris/rpi-sysroot
CC       := $(CROSS_COMPILE)gcc
TARGET   := main

ifneq ($(strip $(SYSROOT)),)
SYSROOT_FLAG := --sysroot=$(SYSROOT)
endif

CPPFLAGS := -I. \
	-IQueue \
	-IThreads \
	-IjsonParse \
	-IWebSocket \
	-I$(SYSROOT)/usr/include/arm-linux-gnueabihf

CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -pthread -g -O0 \
	-march=armv6 -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -marm \
	$(SYSROOT_FLAG)
LDFLAGS  := -march=armv6 -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -marm \
	$(SYSROOT_FLAG)
LDLIBS   := -pthread -lwebsockets -lcjson

SOURCES  := main.c \
	Queue/queue.c \
	Threads/threads.c \
	jsonParse/jsonParse.c \
	WebSocket/websocketHelpers.c \
	WebSocket/wsConnectWrapper.c

OBJECTS  := $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	rm -f $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d)
