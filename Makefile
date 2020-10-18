
vpath %.c src

RM := rm -f

CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -fanalyzer -O0 -ggdb3
CPPFLAGS := -Iinclude -I/usr/include/SDL2 -D_GNU_SOURCE -D_REENTRANT -DNDEBUG
LDFLAGS :=
LDLIBS := -lSDL2 -pthread

SRCS := $(notdir $(wildcard src/*.c))
OBJS := $(patsubst %.c,%.o,$(SRCS))

TARGET := ember

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -o $@ $^ $(LOADLIBES) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)
