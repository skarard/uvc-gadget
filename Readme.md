# uvc-gadget

A new version of uvc-gadget with totally rewriten source code to be more flexible and extensible.

### Available inputs:
- V4L2 capture device (like /dev/video0)
- Framebuffer (like /dev/fb0)
- Image (like image.jpg)

## How to use

    Usage: ./uvc-gadget [options]
    
    Available options are
        -a             Find UVC device automatically
        -b value       Blink X times on startup (b/w 1 and 20 with led0 or GPIO pin if defined)
        -d             Enable debug messages
        -f device      Framebuffer device
        -h             Print this help screen and exit
        -i image       MJPEG/YUYV image
        -l             Use onboard led0 for streaming status indication
        -m value       STDIN stream dimension (WIDTHxHEIGHT like 800x600)
        -n value       Number of Video buffers (b/w 2 and 32)
        -p value       GPIO pin number for streaming status indication
        -r value       Framerate for framebuffer (b/w 1 and 30)
        -s value       STDIN stream type (MJPEG/YUYV)
        -u device      UVC Video Output device
        -v device      V4L2 Video Capture device
        -x             Show fps information

## Examples of usage
Devices are selected according to availability on the Raspberry Pi Zero W

### How to run uvc-gadget manually
**NOTICE: ConfigFS configuration for usb_gadget must be set before running of uvc-gadget.**
#### Video
```
./uvc-gadget -v /dev/video0 -u /dev/video1
```

#### Framebuffer
```
./uvc-gadget -f /dev/fb0 -u /dev/video1 -r 10
```

#### Image
```
./uvc-gadget -i image.jpg -u /dev/video1 -r 5
```

### How to run uvc-gadget with predefined ConfigFS configuration

#### Video
```
# Compressed video at 30 FPS

sudo ./gadget-run.sh conf-camera-mjpeg

# Compressed video at 60 FPS

sudo ./gadget-run.sh conf-camera-mjpeg-60fps

# Uncompressed video

sudo ./gadget-run.sh conf-camera-uncompressed
```

#### Framebuffer
```
sudo ./gadget-run.sh conf-fb
```

#### Image
```
sudo ./gadget-run.sh conf-image
```

### Using video and serial gadget together
```
# Compressed video at 30 FPS + Serial

sudo ./gadget-run.sh conf-camera-mjpeg conf-serial
```

### Cleanup ConfiFS configuration
```
sudo ./gadget-cleanup.sh
```

### Checking of an actual configuration
```
sudo ./gadget-check.sh
```

## Documentation
*UNDER CONSTRUCTION*

### How to define frame resolutions
- [Frame resolutions and formats](doc/frame-resolution.md)

### How to enable more video controls
- [Extended set of video controls](doc/video-controls.md)

### Tree of USB gadget configuration with description
- [USB gadget with UVC camera - configfs](doc/configfs.md)

## Build  
- host:  
    make
- Cross compile:  
    make ARCH=arch CROSS_COMPILE=cross_compiler  
    eg:  
    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-  
- or:  
    set ARCH, CROSS_COMPILE, KERNEL_DIR in Makefile

## Old version
- Original [uvc-gadget.git](http://git.ideasonboard.org/uvc-gadget.git)

        UVC gadget test application
        Copyright (C) 2010 Ideas on board SPRL
        Copyright (C) 2013 ST Microelectronics Ltd.

- Fork(copy) [uvc-gadget.git - wlhe](https://github.com/wlhe/uvc-gadget)
    - Apply patchset [Bugfixes for UVC gadget test application](https://www.spinics.net/lists/linux-usb/msg99220.html)
    - Apply patchset [UVC gadget test application enhancements](https://www.spinics.net/lists/linux-usb/msg84376.html)

- Fork(copy) [uvc-gadget.git - climberhunt / Dave Hunt](https://github.com/climberhunt/uvc-gadget)

- Fork(copy) [uvc-gadget.git - peterbay / Petr Vavrin](https://github.com/peterbay/uvc-gadget)

## Useful resources
- [Raspberry Pi Zero with Pi Camera as USB Webcam / Dave Hunt](http://www.davidhunt.ie/raspberry-pi-zero-with-pi-camera-as-usb-webcam/)
- [TC35874 to UVC gadget application / Kin Wei Lee](https://github.com/kinweilee/v4l2-mmal-uvc)

