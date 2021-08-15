CROSS_COMPILE	?= 

CC			:= $(CROSS_COMPILE)gcc
CFLAGS		:= -W -Wall -g -O3
LDFLAGS		:= -g

OBJFILES = src/configfs.o                   \
			src/processing.o                \
			src/v4l2_endpoint.o             \
			src/fb_endpoint.o               \
			src/image_endpoint.o            \
			src/stdin_endpoint.o            \
			src/processing_actions.o        \
			src/processing_fb_uvc.o         \
			src/processing_image_uvc.o      \
			src/processing_v4l2_uvc.o       \
			src/processing_stdin_uvc.o      \
			src/uvc_device_detect.o         \
			src/uvc_endpoint.o              \
			src/uvc_control.o               \
			src/uvc_events.o                \
			src/uvc_names.o                 \
			src/v4l2_names.o                \
			src/system.o                    \
			src/uvc-gadget.o

TARGET = uvc-gadget

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LDFLAGS)

clean:
	rm -f $(OBJFILES) $(TARGET) *~
