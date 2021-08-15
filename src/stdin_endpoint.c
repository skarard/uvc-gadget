
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
        stdin->buffers[i].length = 4147200;
        stdin->buffers[i].start = malloc(4147200);
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
        stdin->width = width;
        stdin->height = height;

        ret = stdin_buffer_init(processing);
        if (ret == -1)
        {
            return;
        }

        processing->source.type = ENDPOINT_STDIN;
        processing->source.state = true;
        settings->uvc_buffer_required = true;
        settings->uvc_buffer_size = 4147200;
    }
}
