
#include "headers.h"
#include "control_mapping.h"
#include "uvc_names.h"
#include "v4l2_names.h"

static void v4l2_set_ctrl_value(struct processing *processing,
                                struct control_mapping_pair *ctrl,
                                unsigned int ctrl_v4l2,
                                int v4l2_ctrl_value)
{
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;
    memset(&queryctrl, 0, sizeof(struct v4l2_queryctrl));
    char *control_name = v4l2_control_name(ctrl->v4l2);

    queryctrl.id = ctrl_v4l2;
    if (ioctl(v4l2->fd, VIDIOC_QUERYCTRL, &queryctrl) == -1)
    {
        if (errno != EINVAL)
        {
            printf("V4L2: %s VIDIOC_QUERYCTRL failed: %s (%d).\n",
                   control_name, strerror(errno), errno);
        }
        else
        {
            printf("V4L2: %s is not supported: %s (%d).\n",
                   control_name, strerror(errno), errno);
        }
    }
    else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
    {
        printf("V4L2: %s is disabled.\n", control_name);
    }
    else
    {
        memset(&control, 0, sizeof(struct v4l2_control));
        control.id = ctrl->v4l2;
        control.value = v4l2_ctrl_value;

        if (ioctl(v4l2->fd, VIDIOC_S_CTRL, &control) == -1)
        {
            printf("V4L2: %s VIDIOC_S_CTRL failed: %s (%d).\n",
                   control_name, strerror(errno), errno);
            return;
        }
        printf("V4L2: %s changed value (V4L2: %d, UVC: %d)\n",
               control_name, v4l2_ctrl_value, ctrl->value);
    }
}

void v4l2_set_ctrl(struct processing *processing)
{
    struct events *events = &processing->events;
    struct control_mapping_pair *ctrl = events->control;
    int v4l2_ctrl_value = 0;
    int v4l2_diff = 0;
    int ctrl_diff = 0;

    if (ctrl && events->apply_control)
    {
        v4l2_diff = ctrl->v4l2_maximum - ctrl->v4l2_minimum;
        ctrl_diff = ctrl->maximum - ctrl->minimum;
        ctrl->value = clamp(ctrl->value, ctrl->minimum, ctrl->maximum);
        v4l2_ctrl_value = (ctrl->value - ctrl->minimum) * v4l2_diff / ctrl_diff + ctrl->v4l2_minimum;

        v4l2_set_ctrl_value(processing, ctrl, ctrl->v4l2, v4l2_ctrl_value);

        if (ctrl->v4l2 == V4L2_CID_RED_BALANCE)
        {
            v4l2_set_ctrl_value(processing, ctrl, V4L2_CID_BLUE_BALANCE, v4l2_ctrl_value);
        }

        events->control = NULL;
        events->apply_control = false;
    }
}

static void v4l2_prepare_camera_control(struct processing *processing,
                                        struct control_mapping_pair *mapping,
                                        struct v4l2_queryctrl queryctrl,
                                        unsigned int value)
{
    struct controls *controls = &processing->controls;

    controls->mapping[controls->size] = (struct control_mapping_pair){
        .control_type = queryctrl.type,
        .default_value = (0 - queryctrl.minimum) + queryctrl.default_value,
        .enabled = !(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED),
        .maximum = (0 - queryctrl.minimum) + queryctrl.maximum,
        .minimum = 0,
        .step = queryctrl.step,
        .type = mapping->type,
        .uvc = mapping->uvc,
        .v4l2 = mapping->v4l2,
        .v4l2_maximum = queryctrl.maximum,
        .v4l2_minimum = queryctrl.minimum,
        .value = (0 - queryctrl.minimum) + value,
    };

    printf("V4L2: Mapping %s (%s = %s)\n", queryctrl.name, v4l2_control_name(mapping->v4l2),
           uvc_control_name(mapping->type, mapping->uvc));

    printf("V4L2:   V4L2: min: %d, max: %d, step: %d, default: %d, value: %d\n",
           queryctrl.minimum,
           queryctrl.maximum,
           queryctrl.step,
           queryctrl.default_value,
           value);

    printf("V4L2:   UVC:  min: %d, max: %d, step: %d, default: %d, value: %d\n",
           controls->mapping[controls->size].minimum,
           controls->mapping[controls->size].maximum,
           controls->mapping[controls->size].step,
           controls->mapping[controls->size].default_value,
           controls->mapping[controls->size].value);

    controls->size += 1;
}

void v4l2_get_controls(struct processing *processing)
{
    struct controls *controls = &processing->controls;
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control ctrl;
    int i;

    const unsigned next_fl = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;

    memset(&queryctrl, 0, sizeof(struct v4l2_queryctrl));

    controls->size = 0;

    queryctrl.id = next_fl;
    while (ioctl(v4l2->fd, VIDIOC_QUERYCTRL, &queryctrl) == 0)
    {
        for (i = 0; i < control_mapping_size; i++)
        {
            if (control_mapping[i].v4l2 == queryctrl.id)
            {
                ctrl.id = queryctrl.id;
                if (ioctl(v4l2->fd, VIDIOC_G_CTRL, &ctrl) == 0)
                {
                    v4l2_prepare_camera_control(processing, &control_mapping[i], queryctrl,
                                                ctrl.value);
                }
            }
        }
        queryctrl.id |= next_fl;
    }
}

static void v4l2_get_available_formats(struct endpoint_v4l2 *v4l2)
{
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum frmsize;

    memset(&fmtdesc, 0, sizeof(struct v4l2_fmtdesc));
    memset(&frmsize, 0, sizeof(struct v4l2_frmsizeenum));

    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(v4l2->fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
    {
        fmtdesc.index++;
        frmsize.pixel_format = fmtdesc.pixelformat;
        frmsize.index = 0;
        while (ioctl(v4l2->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0)
        {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                printf("V4L2: Highest frame size: %c%c%c%c %ux%u\n",
                       pixfmtstr(fmtdesc.pixelformat), frmsize.discrete.width, frmsize.discrete.height);
            }
            else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
            {
                printf("V4L2: Highest frame size: %c%c%c%c %ux%u\n",
                       pixfmtstr(fmtdesc.pixelformat), frmsize.stepwise.max_width, frmsize.stepwise.max_height);
            }

            frmsize.index++;
        }
    }
}

void v4l2_stream_on(struct processing *processing)
{
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;
    struct v4l2_requestbuffers req;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    unsigned int i = 0;

    if (processing->source.type == ENDPOINT_V4L2 &&
        v4l2->is_streaming == false)
    {
        printf("V4L2: Stream ON init\n");

        v4l2->dqbuf_count = 0;
        v4l2->qbuf_count = 0;

        memset(&req, 0, sizeof(struct v4l2_requestbuffers));
        req.count = v4l2->nbufs;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (ioctl(v4l2->fd, VIDIOC_REQBUFS, &req) < 0)
        {
            printf("V4L2: VIDIOC_REQBUFS error: %s (%d).\n", strerror(errno), errno);
            return;
        }

        v4l2->mem = calloc(req.count, sizeof v4l2->mem[0]);
        if (!v4l2->mem)
        {
            printf("V4L2: Out of memory\n");
            return;
        }

        for (i = 0; i < req.count; ++i)
        {
            memset(&v4l2->mem[i].buf, 0, sizeof(struct v4l2_buffer));
            v4l2->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2->mem[i].buf.memory = V4L2_MEMORY_MMAP;
            v4l2->mem[i].buf.index = i;

            if (ioctl(v4l2->fd, VIDIOC_QUERYBUF, &(v4l2->mem[i].buf)) < 0)
            {
                printf("V4l2: VIDIOC_QUERYBUF failed for buf %d: %s (%d).\n", i, strerror(errno), errno);
                free(v4l2->mem);
                return;
            }

            v4l2->mem[i].start = mmap(NULL, v4l2->mem[i].buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                      v4l2->fd, v4l2->mem[i].buf.m.offset);

            if (MAP_FAILED == v4l2->mem[i].start)
            {
                printf("V4L2: Unable to map buffer %u: %s (%d).\n", i, strerror(errno), errno);
                v4l2->mem[i].length = 0;
                free(v4l2->mem);
                return;
            }

            v4l2->mem[i].length = v4l2->mem[i].buf.length;
            printf("V4L2: Buffer %u mapped at address %p, length %d.\n", i, v4l2->mem[i].start,
                   v4l2->mem[i].length);
        }
        printf("V4L2: %u buffers allocated.\n", req.count);

        for (i = 0; i < v4l2->nbufs; ++i)
        {
            memset(&v4l2->mem[i].buf, 0, sizeof(struct v4l2_buffer));
            v4l2->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2->mem[i].buf.memory = V4L2_MEMORY_MMAP;
            v4l2->mem[i].buf.index = i;

            if (ioctl(v4l2->fd, VIDIOC_QBUF, &(v4l2->mem[i].buf)) < 0)
            {
                printf("V4L2: VIDIOC_QBUF failed : %s (%d).\n", strerror(errno), errno);
                return;
            }
            v4l2->qbuf_count++;
        }

        if (ioctl(v4l2->fd, VIDIOC_STREAMON, &type) < 0)
        {
            printf("V4L2: STREAM ON failed: %s (%d).\n", strerror(errno), errno);
            return;
        }

        printf("V4L2: STREAM ON success\n");
        v4l2->is_streaming = 1;

        return;
    }
}

void v4l2_stream_off(struct processing *processing)
{
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_requestbuffers req;
    unsigned int i;

    if (processing->source.type == ENDPOINT_V4L2 &&
        v4l2->is_streaming == true)
    {
        printf("V4L2: Stream OFF init\n");

        if (ioctl(v4l2->fd, VIDIOC_STREAMOFF, &type) < 0)
        {
            printf("V4L2: STREAM OFF failed: %s (%d).\n", strerror(errno), errno);
            return;
        }

        if (v4l2->mem)
        {
            printf("V4L2: Uninit device\n");

            for (i = 0; i < v4l2->nbufs; ++i)
            {
                if (munmap(v4l2->mem[i].start, v4l2->mem[i].length) < 0)
                {
                    printf("V4L2: munmap failed\n");
                    return;
                }
            }
            free(v4l2->mem);
            v4l2->mem = NULL;
        }

        memset(&req, 0, sizeof(struct v4l2_requestbuffers));
        req.count = 0;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (ioctl(v4l2->fd, VIDIOC_REQBUFS, &req) < 0)
        {
            printf("V4L2: VIDIOC_REQBUFS error: %s (%d).\n", strerror(errno), errno);
            return;
        }

        printf("V4L2: Stream OFF success\n");
        v4l2->is_streaming = false;
    }
}

void v4l2_apply_format(struct processing *processing)
{
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;
    struct events *events = &processing->events;
    struct v4l2_format fmt;

    if (processing->source.type == ENDPOINT_V4L2 && events->apply_frame_format)
    {
        memset(&fmt, 0, sizeof(struct v4l2_format));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.field = V4L2_FIELD_ANY;
        fmt.fmt.pix.width = events->apply_frame_format->wWidth;
        fmt.fmt.pix.height = events->apply_frame_format->wHeight;
        fmt.fmt.pix.pixelformat = events->apply_frame_format->video_format;
        fmt.fmt.pix.sizeimage = fmt.fmt.pix.width * fmt.fmt.pix.height *
                                ((fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) ? 2 : 1);

        if (ioctl(v4l2->fd, VIDIOC_S_FMT, &fmt) < 0)
        {
            printf("V4L2: Unable to set format %s (%d).\n", strerror(errno), errno);
            return;
        }

        printf("V4L2: Setting format: %c%c%c%c %ux%u\n", pixfmtstr(fmt.fmt.pix.pixelformat),
               fmt.fmt.pix.width, fmt.fmt.pix.height);

        memset(&fmt, 0, sizeof(struct v4l2_format));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (ioctl(v4l2->fd, VIDIOC_G_FMT, &fmt) < 0)
        {
            printf("V4L2: Unable to get format %s (%d).\n", strerror(errno), errno);
            return;
        }

        printf("V4L2: Getting format: %c%c%c%c %ux%u\n", pixfmtstr(fmt.fmt.pix.pixelformat),
               fmt.fmt.pix.width, fmt.fmt.pix.height);
    }
}

void v4l2_close(struct processing *processing)
{
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;

    if (processing->source.type == ENDPOINT_V4L2 &&
        v4l2->fd)
    {
        printf("V4L2: Closing %s device\n", v4l2->device_name);
        close(v4l2->fd);
        v4l2->fd = -1;
    }
}

void v4l2_init(struct processing *processing,
               const char *device_name,
               unsigned int nbufs)
{
    struct endpoint_v4l2 *v4l2 = &processing->source.v4l2;
    struct settings *settings = &processing->settings;
    struct v4l2_capability cap;

    if (processing->source.type == ENDPOINT_NONE && device_name)
    {
        printf("V4L2: Opening %s device\n", device_name);

        processing->source.state = true;
        v4l2->device_name = device_name;
        v4l2->fd = open(device_name, O_RDWR | O_NONBLOCK, 0);
        if (v4l2->fd == -1)
        {
            printf("V4L2: Device open failed: %s (%d).\n", strerror(errno), errno);
            return;
        }

        if (ioctl(v4l2->fd, VIDIOC_QUERYCAP, &cap) < 0)
        {
            printf("V4L2: VIDIOC_QUERYCAP failed: %s (%d).\n", strerror(errno), errno);
            goto err;
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        {
            printf("V4L2: %s is no video capture device\n", device_name);
            goto err;
        }

        if (!(cap.capabilities & V4L2_CAP_STREAMING))
        {
            printf("V4L2: %s does not support streaming i/o\n", device_name);
            goto err;
        }

        printf("V4L2: Device is %s on bus %s\n", cap.card, cap.bus_info);

        print_v4l2_capabilities("V4L2", cap.capabilities);

        v4l2->nbufs = nbufs;
        processing->source.type = ENDPOINT_V4L2;
        processing->source.state = true;

        if (!settings->ignore_camera_controls)
        {
            v4l2_get_controls(processing);
        }
        v4l2_get_available_formats(v4l2);
        return;
    }

err:
    v4l2_close(processing);
};
