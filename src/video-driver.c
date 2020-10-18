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

static video_buffer_t* video_buffers = NULL;

video_buffer_t* get_video_buffers(void) {
    return video_buffers;
}

int open_video_stream(const char* device) {
    struct stat st;
    memset(&st, 0, sizeof (st));

    if (stat(device, &st) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "[Error] Device is not a charater device: %s\n", device);
        exit(EXIT_FAILURE);
    }

    int device_descriptor = open(device, O_RDWR | O_NONBLOCK, 0);

    if (device_descriptor == -1) {
        fprintf(stderr, "[Error] Failed to open device: %s (%s)\n", device, strerror(errno));
        exit(EXIT_FAILURE);
    }

    return device_descriptor;
}

void close_video_stream(int device_descriptor) {
    if (close(device_descriptor) == -1) {
        fprintf(stderr, "[Error] Unexpected error while attempting to close stream: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void set_video_buffer_format(int device_descriptor, uint32_t format) {
    struct v4l2_format video_format;
    video_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_format.fmt.pix.pixelformat = format;
    video_format.fmt.pix.height = VIDEO_HEIGHT;
    video_format.fmt.pix.width = VIDEO_WIDTH;
    video_format.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(device_descriptor, VIDIOC_S_FMT, &video_format) == -1) {
        fprintf(stderr, "[Error] Could not set video format: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void set_video_buffer_framerate(int device_descriptor, int frames_per_second) {
    struct v4l2_streamparm stream_parameters;
    memset(&stream_parameters, 0, sizeof (stream_parameters));

    stream_parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    stream_parameters.parm.capture.timeperframe.numerator = 1;
    stream_parameters.parm.capture.timeperframe.denominator = frames_per_second;

    if (ioctl(device_descriptor, VIDIOC_S_PARM, &stream_parameters) == -1) {
        fprintf(stderr, "[Error] Unable to configure video stream frame rate.\n");
        exit(EXIT_FAILURE);
    }
}

void memory_map_video_buffer(int device_descriptor) {
    struct v4l2_requestbuffers request_buffers;
    request_buffers.count = REQUEST_BUFFERS;
    request_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request_buffers.memory = V4L2_MEMORY_MMAP;

    if (ioctl(device_descriptor, VIDIOC_REQBUFS, &request_buffers) == -1) {
        fprintf(stderr, "[Error] Request buffer(s) error.\n");
        exit(EXIT_FAILURE);
    }

    video_buffers = malloc(request_buffers.count * sizeof (video_buffer_t));

    if (video_buffers == NULL) {
        fprintf(stderr, "[Error] Memory allocation failure while instantiating request buffers.\n");
        exit(EXIT_FAILURE);
    }

    struct v4l2_buffer video_buffer;
    size_t n_buffers = 0;

    for (n_buffers = 0; n_buffers < request_buffers.count; ++n_buffers) {
        video_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        video_buffer.memory = V4L2_MEMORY_MMAP;
        video_buffer.index = n_buffers;

        /** Query the video buffers */
        if (ioctl(device_descriptor, VIDIOC_QUERYBUF, &video_buffer) == -1) {
            fprintf(stderr, "[Error] Query buffer error.\n");
            exit(EXIT_FAILURE);
        }

        video_buffers[n_buffers].length = video_buffer.length;

        /** Map the four buffers in driver space to userspace */
        video_buffers[n_buffers].start = mmap(NULL, video_buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, device_descriptor, video_buffer.m.offset);

#ifndef NDEBUG
        /**
         * Output:
         *
         *      Buffer outset: 0        length: 614400
         *      Buffer outset: 614400   length: 614400
         *      Buffer outset: 1228800  length: 614400
         *      Buffer outset: 1843200  length: 614400
         *
         *  Explanation:
         *
         *      Saved in YUV422 format, a pixel needs an
         *      average of two bytes of space. Since our
         *      video frame size is 640 by 480, this gives
         *      us 640 * 480 * 2, or 614400 bytes, per
         *      buffer.
         *
         */
        printf("Buffer outset: %d\t\tlength: %d\n", video_buffer.m.offset, video_buffer.length);
#endif
        if (video_buffers[n_buffers].start == MAP_FAILED) {
            fprintf(stderr, "[Error] Could not map video buffers.\n");
            exit(EXIT_FAILURE);
        }
    }
}

void unmap_video_buffers(void) {
    for (int i = 0; i < REQUEST_BUFFERS; ++i) {
        if (munmap(video_buffers[i].start, video_buffers[i].length) == -1) {
            fprintf(stderr, "[Error] Failed to unmap video buffers.\n");
            exit(EXIT_FAILURE);
        }
    }
}

void start_video_stream(int device_descriptor) {
    struct v4l2_buffer video_buffer;
    video_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_buffer.memory = V4L2_MEMORY_MMAP;

    for (size_t i = 0; i < REQUEST_BUFFERS; ++i) {
        video_buffer.index = i;

        if (ioctl(device_descriptor, VIDIOC_QBUF, &video_buffer) == -1) {
            fprintf(stderr, "[Error] Queue buffer failed.\n");
            exit(EXIT_FAILURE);
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(device_descriptor, VIDIOC_STREAMON, &type) == -1) {
        fprintf(stderr, "[Error] Failed to start video stream.\n");
        exit(EXIT_FAILURE);
    }
}

void stop_video_stream(int device_descriptor) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(device_descriptor, VIDIOC_STREAMOFF, &type) == -1) {
        fprintf(stderr, "[Error] Failed to stop video stream.\n");
        exit(EXIT_FAILURE);
    }
}
