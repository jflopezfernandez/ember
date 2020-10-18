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

#include "video-driver.h"

#include <SDL2/SDL.h>

typedef void (*frame_handler_t)(void* frame, int length);

typedef struct {
    int device_descriptor;
    frame_handler_t frame_handler;
} stream_handler_t;

static pthread_t thread_stream;
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static SDL_Rect render_area;
static int thread_exit_signal = 0;

static void frame_handler(void* frame, __attribute__((unused)) int length) {
    SDL_UpdateTexture(texture, &render_area, frame, VIDEO_WIDTH * 2);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &render_area);
    SDL_RenderPresent(renderer);

#ifdef SAVE_EVERY_FRAME
    static size_t yuv_index = 0;
    char yuvfile[100];
    sprintf(yuvfile, "yuv-%lu.yuv", yuv_index);
    FILE* output_file = fopen(yuvfile, "wb");
    fwrite(frame, length, 1, output_file);
    fclose(output_file);
    ++yuv_index;
#endif
}

static void* video_streaming(void* arg) {
    memset(&render_area, 0, sizeof (render_area));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "[Error] SDL2 initialization failed.\n");
        exit(EXIT_FAILURE);
    }

    window = SDL_CreateWindow(
        "Simple YUV Window",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        VIDEO_WIDTH,
        VIDEO_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (window == NULL) {
        fprintf(stderr, "[Error] Window initialization failed.\n");
        exit(EXIT_FAILURE);
    }

    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (renderer == NULL) {
        fprintf(stderr, "[Error] Renderer initialization failed.\n");
        exit(EXIT_FAILURE);
    }

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_YUY2,
        SDL_TEXTUREACCESS_STREAMING,
        VIDEO_WIDTH,
        VIDEO_HEIGHT
    );

    if (texture == NULL) {
        fprintf(stderr, "[Error] Texture initialization failed.\n");
        exit(EXIT_FAILURE);
    }

    render_area.w = VIDEO_WIDTH;
    render_area.h = VIDEO_HEIGHT;

    int device_descriptor = ((stream_handler_t *)(arg))->device_descriptor;
    void (*handler)(void* frame, int length) =
        ((stream_handler_t *)(arg))->frame_handler;
    
    fd_set device_descriptors;
    struct v4l2_buffer video_buffer;

    while (!thread_exit_signal) {
        FD_ZERO(&device_descriptors);
        FD_SET(device_descriptor, &device_descriptors);

        struct timeval tv = {
            .tv_sec = 1,
            .tv_usec = 0
        };

        int ret = select(device_descriptor + 1, &device_descriptors, NULL, NULL, &tv);

        if (ret == -1) {
            fprintf(stderr, "[Error] %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (ret == 0) {
            fprintf(stderr, "[Error] Timed out waiting for frame.\n");
            continue;
        }

        if (FD_ISSET(device_descriptor, &device_descriptors)) {
            memset(&video_buffer, 0, sizeof (video_buffer));
            video_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            video_buffer.memory = V4L2_MEMORY_MMAP;

            if (ioctl(device_descriptor, VIDIOC_DQBUF, &video_buffer) == -1) {
                fprintf(stderr, "[Error] VIDIOC_DQBUF failure.\n");
                exit(EXIT_FAILURE);
            }

#ifndef NDEBUG
            printf("[Debug] Dequeue buffer[%d]\n", video_buffer.index);
#endif /** NDEBUG */

            if (handler) {
                video_buffer_t* video_buffers = get_video_buffers();
                (*handler)(video_buffers[video_buffer.index].start, video_buffers[video_buffer.index].length);
            }

            video_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            video_buffer.memory = V4L2_MEMORY_MMAP;

            if (ioctl(device_descriptor, VIDIOC_QBUF, &video_buffer) == -1) {
                fprintf(stderr, "[Error] VIDIOC_QBUF failure.\n");
                exit(EXIT_FAILURE);
            }

#ifndef NDEBUG
            printf("[Debug] Queue buffer[%d]\n", video_buffer.index);
#endif /** NDEBUG */
        }
    }

    return NULL;
}

static const char* const device = "/dev/video0";

void clean_up(void) {
    SDL_Quit();
}

int main(void)
{
    /** @todo Accept command-line arguments */

    if (atexit(clean_up)) {
        fprintf(stderr, "[Error] Callback registration failed: clean_up()\n");
        exit(EXIT_FAILURE);
    }
    
    int device_descriptor = open_video_stream(device);
    set_video_buffer_format(device_descriptor, V4L2_PIX_FMT_YUYV);
    set_video_buffer_framerate(device_descriptor, 30);
    memory_map_video_buffer(device_descriptor);
    start_video_stream(device_descriptor);

    /** Create a thread to update the current frame */
    stream_handler_t stream_handler = { device_descriptor, frame_handler };

    if (pthread_create(&thread_stream, NULL, video_streaming, (void *) &stream_handler)) {
        fprintf(stderr, "[Error] Stream-handler thread creation failed.\n");
        stop_video_stream(device_descriptor);
        SDL_Quit();
        exit(EXIT_FAILURE);
    }

    int exit = 0;
    SDL_Event event;

    while (!exit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_KEYDOWN: {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE: {
                            exit = 1;
                        } break;
                    }
                } break;

                case SDL_QUIT: {
                    exit = 1;
                }
            }
        }

        usleep(25);
    }
    
    pthread_join(thread_stream, NULL);

    stop_video_stream(device_descriptor);
    unmap_video_buffers();
    close_video_stream(device_descriptor);

    return EXIT_SUCCESS;
}
