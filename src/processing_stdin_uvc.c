
#include "headers.h"

#include "uvc_events.h"
#include "processing_actions.h"
#include "stdin_endpoint.h"

static void stdin_uvc_video_process(struct processing *processing)
{
    struct endpoint_stdin *stdin = &processing->source.stdin;
    struct endpoint_uvc *uvc = &processing->target.uvc;
    struct settings *settings = &processing->settings;
    struct events *events = &processing->events;
    struct v4l2_buffer uvc_buffer;
    struct stdin_buffer *stdin_buffer = stdin->buffer_use;

    if (!uvc->is_streaming || events->stream == STREAM_OFF || !stdin_buffer->filled)
    {
        return;
    }

    memset(&uvc_buffer, 0, sizeof(struct v4l2_buffer));
    uvc_buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    uvc_buffer.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(uvc->fd, VIDIOC_DQBUF, &uvc_buffer) < 0)
    {
        printf("UVC: Unable to dequeue buffer: %s (%d).\n", strerror(errno), errno);
        return;
    }

    memcpy(uvc->mem[uvc_buffer.index].start, stdin_buffer->start, stdin_buffer->bytesused);
    uvc_buffer.bytesused = stdin_buffer->bytesused;

    if (ioctl(uvc->fd, VIDIOC_QBUF, &uvc_buffer) < 0)
    {
        printf("UVC: Unable to queue buffer: %s (%d).\n", strerror(errno), errno);
        return;
    }

    stdin->fill_buffer = true;
    uvc->qbuf_count++;

    if (settings->show_fps)
    {
        uvc->buffers_processed++;
    }
}

void swap_buffers(struct processing *processing)
{
    struct endpoint_stdin *stdin = &processing->source.stdin;

    unsigned int index_fill = (stdin->buffer_fill->index == 0) ? 1 : 0;

    stdin->buffer_use = stdin->buffer_fill;
    stdin->buffer_use->filled = true;

    stdin->buffer_fill = &stdin->buffers[index_fill];

    stdin->buffer_fill->filled = false;
    stdin->buffer_fill->bytesused = 0;

    stdin->fill_buffer = false;

    printf("STDIN: Buffers swapped, now fill the buffer #%d\n", index_fill);
}

void fill_buffer_from_stdin(struct processing *processing)
{
    struct endpoint_stdin *stdin = &processing->source.stdin;
    struct stdin_buffer *stdin_buffer = stdin->buffer_fill;
    struct settings *settings = &processing->settings;

    unsigned char c = 0;
    unsigned char c_prev = 0;
    unsigned int bytesused = stdin_buffer->bytesused;
    unsigned int limit = stdin_buffer->length - 1;
    unsigned int frame_size = stdin->width * stdin->height * 2;
    unsigned int readed = 0;

    if (bytesused > 0)
    {
        memcpy(&c, stdin_buffer + bytesused - 1, 1);
    }

    if (stdin->stdin_format == V4L2_PIX_FMT_MJPEG)
    {
        while (read(STDIN_FILENO, &c, sizeof(c)) > 0)
        {
            memcpy(stdin_buffer->start + bytesused, &c, 1);
            bytesused += 1;

            if (bytesused == 2)
            {
                if (c != 0xD8 && c_prev != 0xFF)
                {
                    bytesused = 0;
                    stdin_buffer->filled = false;
                    stdin_buffer->bytesused = 0;
                }
            }
            else if (c_prev == 0xFF)
            {
                if (c == 0xD9)
                {
                    swap_buffers(processing);
                    break;
                }
            }

            if (bytesused >= limit)
            {
                bytesused = 0;
            }

            c_prev = c;
        };
    }
    else
    {
        do {
            readed = read(STDIN_FILENO, stdin_buffer->start + bytesused, frame_size - bytesused);
            bytesused += readed;

            if (bytesused == frame_size)
            {
                swap_buffers(processing);
                break;
            }
        }
        while (readed > 0);
    }

    stdin_buffer->bytesused = bytesused;
    if (settings->debug)
    {
        printf("STDIN: Frame readed %d bytes\n", bytesused);
    }
}

void processing_loop_stdin_uvc(struct processing *processing)
{
    struct settings *settings = &processing->settings;
    struct endpoint_stdin *stdin = &processing->source.stdin;
    struct endpoint_uvc *uvc = &processing->target.uvc;
    struct events *events = &processing->events;
    struct stdin_buffer *stdin_buffer = stdin->buffer_fill;

    int activity;
    struct timeval tv;
    double next_frame_time = 0;
    double now;
    fd_set fdsu, fdsi;

    printf("PROCESSING: STDIN %c%c%c%c -> UVC %s\n",
        pixfmtstr(stdin->stdin_format),
        uvc->device_name
    );

    while (!stdin_buffer->filled)
    {
        fill_buffer_from_stdin(processing);
    }

    while (!*(events->terminate))
    {
        FD_ZERO(&fdsu);
        FD_ZERO(&fdsi);

        FD_SET(uvc->fd, &fdsu);
        FD_SET(STDIN_FILENO, &fdsi);

        fd_set efds = fdsu;
        fd_set dfds = fdsu;

        if (stdin->fill_buffer)
        {
            fill_buffer_from_stdin(processing);
        }

        nanosleep((const struct timespec[]){{0, 1000000L}}, NULL);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        activity = select(uvc->fd + 1, NULL, &dfds, &efds, &tv);

        if (activity == -1)
        {
            printf("PROCESSING: Select error %d, %s\n", errno, strerror(errno));
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }

        if (activity == 0)
        {
            printf("PROCESSING: Select timeout\n");
            break;
        }

        if (FD_ISSET(uvc->fd, &efds))
        {
            uvc_events_process(processing);
        }

        now = processing_now();

        if (FD_ISSET(uvc->fd, &dfds))
        {
            if (!*(events->stopped) && now >= next_frame_time)
            {
                stdin_uvc_video_process(processing);

                next_frame_time = now + settings->frame_interval;
            }
        }

        processing_internals(processing);
    }
}