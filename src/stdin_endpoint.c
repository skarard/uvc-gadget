
#include "headers.h"
#include "stdin_endpoint.h"

int stdin_buffer_init(struct processing *processing)
{
    struct endpoint_stdin *stdin = &processing->source.stdin;
    unsigned int i;

    printf("STDIN: Initialize buffers\n");

    stdin->buffers = calloc(2, sizeof stdin->buffers[0]);
    if (!stdin->buffers)
    {
        printf("STDIN: Out of memory\n");
        return -1;
    }

    for (i = 0; i < 2; ++i)
    {
        stdin->buffers[i].index = i;
        stdin->buffers[i].filled = false;
        stdin->buffers[i].length = stdin->buffer_size;
        stdin->buffers[i].start = malloc(stdin->buffer_size);
        if (!stdin->buffers[i].start)
        {
            printf("STDIN: Out of memory\n");
            return -1;
        }
    }

    stdin->buffer_fill = &stdin->buffers[0];
    stdin->buffer_use = &stdin->buffers[0];

    return 0;
}

void stdin_buffer_free(struct processing *processing)
{
    struct endpoint_stdin *stdin = &processing->source.stdin;
    unsigned int i;

    if (stdin->buffers)
    {
        printf("STDIN: Uninit device\n");

        for (i = 0; i < 2; ++i)
        {
            free(stdin->buffers[i].start);
            stdin->buffers[i].start = NULL;
        }
        free(stdin->buffers);
        stdin->buffers = NULL;
    }
}

void swap_buffers(struct processing *processing)
{
    struct endpoint_stdin *stdin = &processing->source.stdin;
    struct settings *settings = &processing->settings;

    unsigned int index_fill = (stdin->buffer_fill->index == 0) ? 1 : 0;

    stdin->buffer_use = stdin->buffer_fill;
    stdin->buffer_use->filled = true;

    stdin->buffer_fill = &stdin->buffers[index_fill];

    stdin->buffer_fill->filled = false;
    stdin->buffer_fill->bytesused = 0;

    stdin->fill_buffer = false;

    if (settings->debug)
    {
        printf("STDIN: Buffers swapped, now fill the buffer #%d\n", index_fill);
    }
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

    if (stdin->stdin_format == V4L2_PIX_FMT_MJPEG)
    {
        if (bytesused > 0)
        {
            memcpy(&c, stdin_buffer + bytesused - 1, 1);
        }

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

void stdin_get_first_frame(struct processing *processing)
{
    struct endpoint_stdin *stdin = &processing->source.stdin;
    struct events *events = &processing->events;
    struct stdin_buffer *stdin_buffer = stdin->buffer_fill;

    int activity;
    struct timeval tv;
    fd_set fdsi;

    printf("STDIN: Waiting for first frame\n");

    while (!*(events->terminate) && !stdin_buffer->filled)
    {
        FD_ZERO(&fdsi);
        FD_SET(STDIN_FILENO, &fdsi);
 
        nanosleep((const struct timespec[]){{0, 1000000L}}, NULL);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        activity = select(STDIN_FILENO + 1, &fdsi, NULL, NULL, &tv);

        if (activity == -1)
        {
            printf("STDIN: Select error %d, %s\n", errno, strerror(errno));
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }

        if (activity == 0)
        {
            printf("STDIN: Select timeout\n");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &fdsi))
        {
            fill_buffer_from_stdin(processing);
        }
    }


    FILE *fp = fopen("first_frame", "w");
    if (fp)
    {
        fwrite(stdin_buffer->start, sizeof(char), stdin_buffer->bytesused, fp);
        fclose(fp);
    }
}

void stdin_init(struct processing *processing,
                unsigned int stdin_format,
                unsigned int width,
                unsigned int height
                )
{
    struct endpoint_stdin *stdin = &processing->source.stdin;
    struct settings *settings = &processing->settings;
    int ret;

    if (processing->source.type == ENDPOINT_NONE && stdin_format)
    {
        printf("STDIN: Initialize stdin\n");

        stdin->stdin_format = stdin_format;
        stdin->buffer_size = width * height * 2;
        stdin->width = width;
        stdin->height = height;

        ret = stdin_buffer_init(processing);
        if (ret == -1)
        {
            return;
        }

        stdin_get_first_frame(processing);

        if (!stdin->buffer_fill)
        {
            stdin_buffer_free(processing);
            return;
        }

        stdin->fill_buffer = true;
        processing->source.type = ENDPOINT_STDIN;
        processing->source.state = true;
        settings->uvc_buffer_required = true;
        settings->uvc_buffer_size = stdin->buffer_size;
    }
}
