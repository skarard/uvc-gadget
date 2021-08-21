
#ifndef HEADERS
#define HEADERS

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <ftw.h>
#include <dirent.h>

#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <linux/usb/ch9.h>
#include <linux/usb/video.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <linux/uvcvideo.h>

#define max(a, b) (((a) > (b)) ? (a) : (b))

#define clamp(val, min, max)                   \
    ({                                         \
        typeof(val) __val = (val);             \
        typeof(min) __min = (min);             \
        typeof(max) __max = (max);             \
        (void)(&__val == &__min);              \
        (void)(&__val == &__max);              \
        __val = __val < __min ? __min : __val; \
        __val > __max ? __max : __val;         \
    })

#define pixfmtstr(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

#define UVC_INTF_CONTROL 0
#define UVC_INTF_STREAMING 1

#define UVC_EVENT_FIRST (V4L2_EVENT_PRIVATE_START + 0)
#define UVC_EVENT_CONNECT (V4L2_EVENT_PRIVATE_START + 0)
#define UVC_EVENT_DISCONNECT (V4L2_EVENT_PRIVATE_START + 1)
#define UVC_EVENT_STREAMON (V4L2_EVENT_PRIVATE_START + 2)
#define UVC_EVENT_STREAMOFF (V4L2_EVENT_PRIVATE_START + 3)
#define UVC_EVENT_SETUP (V4L2_EVENT_PRIVATE_START + 4)
#define UVC_EVENT_DATA (V4L2_EVENT_PRIVATE_START + 5)
#define UVC_EVENT_LAST (V4L2_EVENT_PRIVATE_START + 5)

// UVC - Request Error Code Control
#define REQEC_NO_ERROR 0x00
#define REQEC_NOT_READY 0x01
#define REQEC_WRONG_STATE 0x02
#define REQEC_POWER 0x03
#define REQEC_OUT_OF_RANGE 0x04
#define REQEC_INVALID_UNIT 0x05
#define REQEC_INVALID_CONTROL 0x06
#define REQEC_INVALID_REQUEST 0x07
#define REQEC_INVALID_VALUE 0x08

struct uvc_request_data
{
    __s32 length;
    __u8 data[60];
};

struct uvc_event
{
    union
    {
        enum usb_device_speed speed;
        struct usb_ctrlrequest request;
        struct uvc_request_data data;
    };
};

#define UVCIOC_SEND_RESPONSE _IOW('U', 1, struct uvc_request_data)

/* Buffer representing one video frame */
struct buffer
{
    struct v4l2_buffer buf;
    void *start;
    size_t length;
};

struct stdin_buffer {
    void *start;
    size_t length;
    unsigned int bytesused;
    unsigned int index;
    bool filled;
};

enum endpoint_type
{
    ENDPOINT_NONE,
    ENDPOINT_V4L2,
    ENDPOINT_UVC,
    ENDPOINT_FB,
    ENDPOINT_IMAGE,
    ENDPOINT_STDIN
};

enum stream_action
{
    STREAM_NONE,
    STREAM_ON,
    STREAM_OFF
};

enum stream_control_action
{
    STREAM_CONTROL_INIT,
    STREAM_CONTROL_MIN,
    STREAM_CONTROL_MAX,
    STREAM_CONTROL_SET,
};

// CONTROLS
struct control_mapping_pair
{
    unsigned int type;
    unsigned int uvc;
    unsigned int v4l2;
    bool enabled;
    unsigned int control_type;
    unsigned int value;
    unsigned int length;
    unsigned int minimum;
    unsigned int maximum;
    unsigned int step;
    unsigned int default_value;
    int v4l2_minimum;
    int v4l2_maximum;
};

struct controls
{
    struct control_mapping_pair mapping[50];
    int size;
    int index;
};

// ENDPOINTS
struct endpoint_v4l2
{
    const char *device_name;
    int fd;
    int is_streaming;
    struct buffer *mem;
    unsigned int nbufs;
    unsigned long long int dqbuf_count;
    unsigned long long int qbuf_count;
};

struct endpoint_fb
{
    const char *device_name;
    int fd;
    int framerate;
    unsigned int bpp;
    unsigned int height;
    unsigned int line_length;
    unsigned int mem_size;
    unsigned int screen_size;
    unsigned int width;
    void *memory;
};

struct endpoint_uvc
{
    const char *device_name;
    int fd;
    struct buffer *mem;
    struct buffer *dummy_buf;
    unsigned int nbufs;

    unsigned long long int qbuf_count;
    unsigned long long int dqbuf_count;

    double last_time_video_process;
    int buffers_processed;

    int is_streaming;

    struct uvc_streaming_control probe;
    struct uvc_streaming_control commit;
};

struct endpoint_image
{
    const char *image_path;
    unsigned int size;
    void *data;
    int inotify_fd;
};

struct endpoint_stdin
{
    unsigned int stdin_format;
    unsigned int width;
    unsigned int height;
    struct stdin_buffer *buffer_use;
    struct stdin_buffer *buffer_fill;
    struct stdin_buffer *buffers;
};

struct endpoint
{
    enum endpoint_type type;
    struct endpoint_v4l2 v4l2;
    struct endpoint_uvc uvc;
    struct endpoint_fb fb;
    struct endpoint_image image;
    struct endpoint_stdin stdin;
    int state;
};

// UVC REQUEST
struct uvc_request
{
    struct v4l2_event event;
    struct uvc_request_data response;
    struct uvc_request_data data;
    unsigned char response_code;

    uint8_t request;
    uint8_t type;
    uint8_t interface;
    uint8_t control;
    uint8_t length;

    unsigned int set_control;
};

// FRAMEFORMAT
struct frame_format
{
    bool defined;

    enum usb_device_speed usb_speed;
    int video_format;
    const char *format_name;

    unsigned int bFormatIndex;
    unsigned int bFrameIndex;

    unsigned int dwDefaultFrameInterval;
    unsigned int dwMaxVideoFrameBufferSize;
    unsigned int dwMaxBitRate;
    unsigned int dwMinBitRate;
    unsigned int wHeight;
    unsigned int wWidth;
    unsigned int bmCapabilities;
    unsigned int dwFrameInterval;
};

enum frame_format_getter
{
    FORMAT_INDEX_MIN,
    FORMAT_INDEX_MAX,
    FRAME_INDEX_MIN,
    FRAME_INDEX_MAX,
};

// EVENTS
struct events
{
    enum stream_action stream;
    bool shutdown_requested;
    struct frame_format *apply_frame_format;
    struct control_mapping_pair *control;
    bool apply_control;
};

// CONFIGFS
struct streaming
{
    unsigned int maxburst;
    unsigned int maxpacket;
    unsigned int interval;
};

struct configfs
{
    struct streaming streaming;
    struct frame_format frame_format[30];
    unsigned int frame_format_size;
};

// SETTINGS
struct settings
{
    bool debug;
    bool show_fps;
    bool streaming_status_onboard;
    bool streaming_status_onboard_enabled;
    bool streaming_status_enabled;
    char *streaming_status_pin;
    unsigned int blink_on_startup;
    int frame_interval;
    bool uvc_buffer_required;
    unsigned int uvc_buffer_size;
};

// INTERNALS
struct internals
{
    double last_time_blink;
    bool blink_state;
};

// PROCESSING
struct processing
{
    struct endpoint source;
    struct endpoint target;
    struct configfs configfs;
    struct controls controls;
    struct events events;
    struct settings settings;
    struct internals internals;
    struct uvc_request uvc_request;

    bool *terminate;
};

// RGB to YUYV

struct px24 {
    uint8_t ignore1 : 1;
    uint8_t r : 7;
    uint8_t ignore2 : 1;
    uint8_t g : 7;
    uint8_t ignore3 : 1;
    uint8_t b : 7;
};

struct py24 {
    uint8_t ignore1 : 2;
    uint8_t r : 6;
    uint8_t ignore2 : 1;
    uint8_t g : 7;
    uint8_t ignore3 : 3;
    uint8_t b : 5;
};

struct rgb_pixel24 {
    union {
        struct px24 x1;
        struct py24 y1;
        unsigned int rgba1;
    };
    union {
        struct px24 x2;
        struct py24 y2;
        unsigned int rgba2;
    };
};

struct px32 {
    uint8_t ignore1 : 1;
    uint8_t r : 7;
    uint8_t ignore2 : 1;
    uint8_t g : 7;
    uint8_t ignore3 : 1;
    uint8_t b : 7;
    uint8_t a;
};

struct py32 {
    uint8_t ignore1 : 2;
    uint8_t r : 6;
    uint8_t ignore2 : 1;
    uint8_t g : 7;
    uint8_t ignore3 : 3;
    uint8_t b : 5;
    uint8_t a;
};

struct rgb_pixel32 {
    union {
        struct px32 x1;
        struct py32 y1;
        unsigned int rgba1;
    };
    union {
        struct px32 x2;
        struct py32 y2;
        unsigned int rgba2;
    };
};

struct yuyv_pixel {
    union {
        struct z {
            uint8_t y1;
            uint8_t u;
            uint8_t y2;
            uint8_t v;
        } z;
        unsigned int yuyv;
    };
};


#endif // end HEADERS
