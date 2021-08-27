
#include "headers.h"
#include "processing_fb_uvc.h"
#include "processing_image_uvc.h"
#include "processing_v4l2_uvc.h"
#include "processing_stdin_uvc.h"
#include "processing_actions.h"

static bool terminate = false;
static bool stopped = false;

void onSignal(int signum)
{
    switch (signum)
    {
    case SIGTERM:
    case SIGINT:
        printf("\nSIGNAL: Signal %s\n", (signum == SIGTERM) ? "TERMINATE" : "INTERRUPT");
        terminate = true;
        break;

    case SIGUSR1:
        printf("\nSIGNAL: Signal USER-DEFINED 1, STOP STREAMING%s\n", (stopped) ? " - Already stopped" : "");
        stopped = true;
        break;

    case SIGUSR2:
        printf("\nSIGNAL: Signal USER-DEFINED 2, RESUME STREAMING%s\n", (!stopped) ? " - Already running" : "");
        stopped = false;
        break;

    default:
        break;
    }
}

void processing_loop(struct processing *processing)
{
    struct events *events = &processing->events;

    signal(SIGTERM, onSignal);
    signal(SIGINT, onSignal);
    signal(SIGUSR1, onSignal);
    signal(SIGUSR2, onSignal);

    events->terminate = &terminate;
    events->stopped = &stopped;
 
    if (processing->target.type == ENDPOINT_UVC)
    {
        switch (processing->source.type)
        {
        case ENDPOINT_FB:
            processing_loop_fb_uvc(processing);
            break;

        case ENDPOINT_IMAGE:
            processing_loop_image_uvc(processing);
            break;

        case ENDPOINT_V4L2:
            processing_loop_v4l2_uvc(processing);
            break;

        case ENDPOINT_STDIN:
            processing_loop_stdin_uvc(processing);
            break;

        default:
            printf("PROCESSING: ERROR - Missing loop for UVC endpoint\n");
            break;
        }
    }
    else
    {
        printf("PROCESSING: ERROR - Missing loop\n");
    }
}
