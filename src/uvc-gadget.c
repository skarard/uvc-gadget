
#include "headers.h"

#include "configfs.h"
#include "processing.h"
#include "v4l2_endpoint.h"
#include "uvc_endpoint.h"
#include "fb_endpoint.h"
#include "image_endpoint.h"
#include "stdin_endpoint.h"
#include "uvc_events.h"
#include "uvc_control.h"
#include "uvc_device_detect.h"
#include "system.h"

struct processing processing;
bool show_fps = false;
bool debug = false;
bool streaming_status_onboard = false;
bool autodetect_uvc_device = false;
const char *fb_device_name;
const char *uvc_device_name;
const char *v4l2_device_name;
const char *image_path;
unsigned int stdin_format;
unsigned int stdin_width;
unsigned int stdin_height;
char *streaming_status_pin;
int blink_on_startup = 0;
int fb_framerate = 60;
int nbufs = 2;

void cleanup()
{
    fb_close(&processing);
    image_close(&processing);
    v4l2_close(&processing);
    uvc_close(&processing);
    stdin_buffer_free(&processing);

    printf("UVC-GADGET: Exit\n");
}

int init()
{
    const char *uvc_device;

    memset(&processing, 0, sizeof(struct processing));

    processing.source.type = ENDPOINT_NONE;
    processing.target.type = ENDPOINT_NONE;

    processing.events.stream = STREAM_NONE;
    processing.events.shutdown_requested = false;

    processing.settings.debug = debug;
    processing.settings.show_fps = show_fps;
    processing.settings.blink_on_startup = blink_on_startup;
    processing.settings.streaming_status_pin = streaming_status_pin;
    processing.settings.streaming_status_onboard = streaming_status_onboard;
    processing.settings.frame_interval = (1000 / fb_framerate);

    printf("UVC-GADGET: Initialization\n");

    if (configfs_init(&processing, "/sys/kernel/config/usb_gadget") != 1)
    {
        goto err;
    }

    v4l2_init(&processing, v4l2_device_name, nbufs);
    fb_init(&processing, fb_device_name);
    image_init(&processing, image_path);
    stdin_init(&processing, stdin_format, stdin_width, stdin_height);

    if (uvc_device_name)
    {
        uvc_init(&processing, uvc_device_name, nbufs);
    }
    else if (autodetect_uvc_device)
    {
        uvc_device = uvc_device_detect();
        if (uvc_device)
        {
            printf("UVC: Autodetect found UVC device: %s\n", uvc_device);
        }
        uvc_init(&processing, uvc_device, nbufs);
    }

    system_init(&processing);

    if (processing.source.type == ENDPOINT_NONE)
    {
        printf("ERROR: Missing source endpoint\n");
        goto err;
    }

    if (processing.target.type == ENDPOINT_NONE)
    {
        printf("ERROR: Missing target endpoint\n");
        goto err;
    }

    if (processing.target.type == ENDPOINT_UVC)
    {
        uvc_fill_streaming_control(&processing, &(processing.target.uvc.probe), STREAM_CONTROL_INIT, NULL);
        uvc_fill_streaming_control(&processing, &(processing.target.uvc.commit), STREAM_CONTROL_INIT, NULL);

        uvc_events_subscribe(&processing);
    }

    processing_loop(&processing);

    if (processing.target.type == ENDPOINT_UVC)
    {
        uvc_events_unsubscribe(&processing);
    }

    cleanup();
    return 0;

err:
    cleanup();
    return 1;
}

static void usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "Available options are\n");
    fprintf(stderr, " -a          Find UVC device automatically\n");
    fprintf(stderr, " -b value    Blink X times on startup (b/w 1 and 20 with led0 or GPIO pin if defined)\n");
    fprintf(stderr, " -d          Enable debug messages\n");
    fprintf(stderr, " -f device   Framebuffer device\n");
    fprintf(stderr, " -h          Print this help screen and exit\n");
    fprintf(stderr, " -i image    MJPEG/YUYV image\n");
    fprintf(stderr, " -l          Use onboard led0 for streaming status indication\n");
    fprintf(stderr, " -m value    STDIN stream dimension (WIDTHxHEIGHT like 800x600)\n");
    fprintf(stderr, " -n value    Number of Video buffers (b/w 2 and 32)\n");
    fprintf(stderr, " -p value    GPIO pin number for streaming status indication\n");
    fprintf(stderr, " -r value    Framerate for framebuffer (b/w 1 and 60)\n");
    fprintf(stderr, " -s value    STDIN stream type (MJPEG/YUYV)\n");
    fprintf(stderr, " -u device   UVC Video Output device\n");
    fprintf(stderr, " -v device   V4L2 Video Capture device\n");
    fprintf(stderr, " -x          Show fps information\n");
}

int main(int argc, char *argv[])
{
    int opt;

    while ((opt = getopt(argc, argv, "adhlb:f:i:m:n:p:r:s:u:v:x")) != -1)
    {
        switch (opt)
        {
        case 'a':
            autodetect_uvc_device = true;
            break;

        case 'b':
            if (atoi(optarg) < 1 || atoi(optarg) > 20)
            {
                fprintf(stderr, "ERROR: Blink x times on startup\n");
                goto err;
            }
            blink_on_startup = atoi(optarg);
            break;

        case 'd':
            debug = true;
            break;

        case 'f':
            fb_device_name = optarg;
            break;

        case 'h':
            usage(argv[0]);
            return 1;

        case 'i':
            image_path = optarg;
            break;

        case 'l':
            streaming_status_onboard = true;
            break;

        case 'm':
            if (2 != sscanf(optarg, "%dx%d", &stdin_width, &stdin_height))
            {
                fprintf(stderr, "ERROR: Wrong input stream dimension - %s\n", optarg);
                goto err;
            }
            break;

        case 'n':
            if (atoi(optarg) < 2 || atoi(optarg) > 32)
            {
                fprintf(stderr, "ERROR: Number of Video buffers value out of range\n");
                goto err;
            }
            nbufs = atoi(optarg);
            break;

        case 'p':
            streaming_status_pin = optarg;
            break;

        case 'r':
            if (atoi(optarg) < 1 || atoi(optarg) > 60)
            {
                fprintf(stderr, "ERROR: Framerate value out of range\n");
                goto err;
            }
            fb_framerate = atoi(optarg);
            break;

        case 's':
            if (!strncmp(optarg, "MJPEG", 5))
            {
                stdin_format = V4L2_PIX_FMT_MJPEG;
            }
            else if (!strncmp(optarg, "YUYV", 4))
            {
                stdin_format = V4L2_PIX_FMT_YUYV;
            }
            else
            {
                fprintf(stderr, "ERROR: Wrong input stream type\n");
                goto err;
            }
            break;

        case 'u':
            uvc_device_name = optarg;
            break;

        case 'v':
            v4l2_device_name = optarg;
            break;

        case 'x':
            show_fps = true;
            break;

        default:
            printf("ERROR: Invalid option '-%c'\n", opt);
            return 1;
        }
    }
    return init();

err:
    usage(argv[0]);
    return 1;
}
