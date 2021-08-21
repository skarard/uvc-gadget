
#include "headers.h"
#include "image_endpoint.h"

int image_load(struct processing *processing)
{
    struct endpoint_image *image = &processing->source.image;
    void *data;
    int fd;

    if (image->data)
    {
        free(image->data);
        image->data = NULL;
    }

    fd = open(image->image_path, O_RDONLY);
    if (fd == -1)
    {
        printf("IMAGE: Unable to open MJPEG image '%s'\n", image->image_path);
        return -1;
    }

    image->size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    data = malloc(image->size);
    if (data == NULL)
    {
        printf("IMAGE: Unable to allocate memory for MJPEG image\n");
        image->size = 0;
        return -1;
    }

    read(fd, data, image->size);
    close(fd);

    image->data = data;

    return 0;
}

void image_close(struct processing *processing)
{
    struct endpoint_image *image = &processing->source.image;

    if (processing->source.type == ENDPOINT_IMAGE && image->data)
    {
        printf("IMAGE: Closing image\n");
        inotify_rm_watch(image->inotify_fd, IN_CLOSE_WRITE);
        free(image->data);
        image->data = NULL;
    }
}

void image_init(struct processing *processing,
                const char *image_path)
{
    struct endpoint_image *image = &processing->source.image;
    struct settings *settings = &processing->settings;

    if (processing->source.type == ENDPOINT_NONE && image_path)
    {
        printf("IMAGE: Opening image %s\n", image_path);

        image->image_path = image_path;

        if (image_load(processing) < 0)
        {
            printf("IMAGE: ERROR opening image\n");
            return;
        }

        image->inotify_fd = inotify_init();
        inotify_add_watch(image->inotify_fd, image_path, IN_CLOSE_WRITE);

        processing->source.type = ENDPOINT_IMAGE;
        processing->source.state = true;
        settings->uvc_buffer_required = true;
        settings->uvc_buffer_size = 4147200;
    }
}
