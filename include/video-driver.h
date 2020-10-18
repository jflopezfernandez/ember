/**
 * Ember - Real-time video communication.
 * Copyright (C) 2020 Jose Fernando Lopez Fernandez
 * 
 * This program is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 *
 */

#ifndef PROJECT_INCLUDES_EMBER_VIDEO_DRIVER_H
#define PROJECT_INCLUDES_EMBER_VIDEO_DRIVER_H

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <pthread.h>

#include <linux/videodev2.h>

#ifndef REQUEST_BUFFERS
#define REQUEST_BUFFERS 4
#endif

#ifndef VIDEO_HEIGHT
#define VIDEO_HEIGHT 480
#endif

#ifndef VIDEO_WIDTH
#define VIDEO_WIDTH 640
#endif

typedef struct {
    void* start;
    size_t length;
} video_buffer_t;

video_buffer_t* get_video_buffers(void);

int open_video_stream(const char* device);

void close_video_stream(int device_descriptor);

void set_video_buffer_format(int device_descriptor, uint32_t format);

void set_video_buffer_framerate(int device_descriptor, int frames_per_second);

void memory_map_video_buffer(int device_descriptor);

void unmap_video_buffers(void);

void start_video_stream(int device_descriptor);

void stop_video_stream(int device_descriptor);

#endif /** PROJECT_INCLUDES_EMBER_VIDEO_DRIVER_H */
