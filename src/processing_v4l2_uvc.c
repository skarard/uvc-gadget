
#include "headers.h"
#include "uvc_events.h"
#include "processing_actions.h"

static void uvc_v4l2_video_process(struct processing *processing)
{
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;
    struct endpoint_uvc *uvc = &processing->target.uvc;
    struct events *events = &processing->events;
    struct settings *settings = &processing->settings;
    struct v4l2_buffer ubuf;
    struct v4l2_buffer vbuf;

    if ((!events->shutdown_requested && ((uvc->dqbuf_count + 1) >= uvc->qbuf_count)) ||
        events->stream == STREAM_OFF)
    {
        return;
    }

    memset(&ubuf, 0, sizeof(struct v4l2_buffer));
    ubuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ubuf.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(uvc->fd, VIDIOC_DQBUF, &ubuf) < 0)
    {
        printf("UVC: Unable to dequeue buffer: %s (%d).\n", strerror(errno), errno);
        return;
    }

    uvc->dqbuf_count++;

    if (ubuf.flags & V4L2_BUF_FLAG_ERROR)
    {
        events->shutdown_requested = true;
        printf("UVC: Possible USB shutdown requested from Host, seen during VIDIOC_DQBUF\n");
        return;
    }

    memset(&vbuf, 0, sizeof(struct v4l2_buffer));
    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;
    vbuf.index = ubuf.index;
    if (ioctl(v4l2->fd, VIDIOC_QBUF, &vbuf) < 0)
    {
        printf("V4L2: Unable to queue buffer: %s (%d).\n", strerror(errno), errno);
        return;
    }

    v4l2->qbuf_count++;

    if (settings->show_fps)
    {
        uvc->buffers_processed++;
    }
}

static void v4l2_uvc_video_process(struct processing *processing)
{
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;
    struct endpoint_uvc *uvc = &processing->target.uvc;
    struct events *events = &processing->events;
    struct settings *settings = &processing->settings;
    struct v4l2_buffer vbuf;
    struct v4l2_buffer ubuf;

    if (uvc->is_streaming && v4l2->dqbuf_count >= v4l2->qbuf_count)
    {
        return;
    }

    memset(&vbuf, 0, sizeof(struct v4l2_buffer));
    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(v4l2->fd, VIDIOC_DQBUF, &vbuf) < 0)
    {
        printf("V4L2: Unable to dequeue buffer: %s (%d).\n", strerror(errno), errno);
        return;
    }

    v4l2->dqbuf_count++;

    memset(&ubuf, 0, sizeof(struct v4l2_buffer));
    ubuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ubuf.memory = V4L2_MEMORY_USERPTR;
    ubuf.m.userptr = (unsigned long)v4l2->mem[vbuf.index].start;
    ubuf.length = v4l2->mem[vbuf.index].length;
    ubuf.index = vbuf.index;
    ubuf.bytesused = vbuf.bytesused;
    if (ioctl(uvc->fd, VIDIOC_QBUF, &ubuf) < 0)
    {
        if (errno == ENODEV)
        {
            events->shutdown_requested = true;
            printf("UVC: Possible USB shutdown requested from Host, seen during VIDIOC_QBUF\n");
        }
        return;
    }

    uvc->qbuf_count++;

    if (!uvc->is_streaming)
    {
        processing->events.stream = STREAM_ON;
        settings->blink_on_startup = 0;
    }
}

void processing_loop_v4l2_uvc(struct processing *processing)
{
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;
    struct endpoint_uvc *uvc = &processing->target.uvc;
    struct events *events = &processing->events;
    struct timeval tv;
    int activity;
    fd_set fdsv, fdsu;

    printf("PROCESSING: V4L2 %s -> UVC %s\n", v4l2->device_name, uvc->device_name);

    while (!*(events->terminate))
    {
        FD_ZERO(&fdsv);
        FD_ZERO(&fdsu);

        FD_SET(uvc->fd, &fdsu);
        FD_SET(v4l2->fd, &fdsv);

        fd_set efds = fdsu;
        fd_set dfds = fdsu;

        nanosleep((const struct timespec[]){{0, 1000000L}}, NULL);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (v4l2->is_streaming)
        {
            activity = select(max(v4l2->fd, uvc->fd) + 1, &fdsv, &dfds, &efds, &tv);

            if (activity == 0)
            {
                printf("PROCESSING: Select timeout\n");
                break;
            }
        }
        else
        {
            activity = select(uvc->fd + 1, NULL, &dfds, &efds, NULL);
        }

        if (activity == -1)
        {
            printf("PROCESSING: Select error %d, %s\n", errno, strerror(errno));
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }

        if (FD_ISSET(uvc->fd, &efds))
        {
            uvc_events_process(processing);
        }

        if (v4l2->is_streaming && !*(events->stopped))
        {
            if (FD_ISSET(uvc->fd, &dfds))
            {
                uvc_v4l2_video_process(processing);
            }

            if (FD_ISSET(v4l2->fd, &fdsv))
            {
                v4l2_uvc_video_process(processing);
            }
        }

        processing_internals(processing);
    }
}
