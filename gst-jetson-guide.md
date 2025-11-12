# GStreamer Guide by carpetto

## GStreamer Tools

### gst-launch-1.0

Simple debugging tool, used to verify if the system is functioning.

Elements added to this tool are seperated with !

Basic animated video patterns:
```
gst-launch-1.0 videotestsrc ! videoconvert ! autovideosink
```
```
gst-launch-1.0 videotestsrc pattern=11 ! videoconvert ! autovideosink
```
```
gst-launch-1.0 videotestsrc ! videoconvert ! tee name=t ! queue! autovideosink t. ! queue ! autovideosink
```

### gst-inspect-1.0

Without any argument this lists all available elements types.

You can search for specific element like:

```
gst-inspect-1.0 | grep "nv"
```

Normally Jetson is built with Nvidia (nv) Gstreamer API but sometimes they are not correctly installed. 

To fix that go to the [Fix Jetson Gstreamer API](#fix-jetson-gstreamer-api) sub-page.

To find all the information regarding an element:

```
gst-inspect-1.0 nv3dsink
```


### gst-device-monitor-1.0

Lists all the Gstreamer devices.

To make it follow devices that are being attached:

```
gst-device-monitor-1.0
```
To list a certain device, pass in the device class parameters:

```
gst-device-monitor-1.0 Video/Source
gst-device-monitor-1.0 Video/Sink
gst-device-monitor-1.0 Audio/Source
gst-device-monitor-1.0 Audio/Sink
```

## Using Nvidia Gstreamer Plugins

Below elements are Nvidia proprietary. For common elements check the below link:

[Gstreamer Plugins](https://gstreamer.freedesktop.org/documentation/plugins_doc.html)

### Commonly used elements

- queue
- tee
- v4l2src
- xvimagesink

### Video source elements

- **nvv4l2camerasrc** :Camera plugin for V4L2 API
- **nveglstreamsrc** :Acts as GStreamer Source Component, accepts EGLStream from EGLStream producer
- **nvarguscamerasrc** :Camera plugin for ARGUS API

### Formatting elements

- **nvvidconv :**Video format conversion and scaling
- **nvegltransform :**Video transform element for NVMM to EGLimage (supported with nveglglessink only)

### Video sink elements

- **nv3dsink** :EGL/GLES video sink element
- **nvdrmvideosink** :DRM video sink element
- **nvvideosink** :Video Sink Component. Accepts YUV-I420 format and produces EGLStream (RGBA)
- **nveglglessink** :EGL/GLES video sink element, support both the X11 and Wayland backends

### Nvidia Gstreamer Tools

#### nvgstcapture-1.0

Only works on cameras with ARGUS API supported. Uses nvarguscamerasrc as video source element. Our camera module does not support this, so it won't be explained.

## Example Pipelines for Camera Capturing

Before capturing set the DISPLAY enviroment variable, this puts the video output to the first monitor connected to the Jetson.

```
export DISPLAY=:0
```

To check the available formats use the gst-device-monitor-1.0 tool

```
gst-device-monitor-1.0 Video/Source
```

Below pipeline uses v4l2src generic source element. This element copies the video data to the RAM using the CPU then it reads from the RAM and outputs to the display using the generic xvimagesink video sink element.

```
gst-launch-1.0 v4l2src device=/dev/video0 ! "video/x-raw, format=UYVY, width=1920, height=1080, framerate=60/1" ! xvimagesink
```

Below pipeline uses nvv4l2camerasrc proprietary source element. This element copies the video directly to the GPU memory. nvvidconv converts the image format to NV12 which is the format nv3dsink element supports. Then the proprietary nv3dsink video sink element displays the video to the screen. This is a faster, lower CPU usage and lower latency format.

Queues achieve multi-threading which provide even less latency.

```
gst-launch-1.0 nvv4l2camerasrc device=/dev/video0 ! 'video/x-raw(memory:NVMM), format=UYVY, width=1920, height=1080, framerate=60/1' ! queue ! nvvidconv ! 'video/x-raw(memory:NVMM), format=NV12' ! queue ! nv3dsink sync=false -e
```

## Fix Jetson Gstreamer API

Firstly install base & Nvidia plugins:
```
sudo apt-get install --reinstall gstreamer1.0-tools gstreamer1.0-alsa \
gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly \
gstreamer1.0-libav
```
```
sudo apt-get install libgstreamer1.0-dev \
     libgstreamer-plugins-base1.0-dev \
     libgstreamer-plugins-good1.0-dev \
     libgstreamer-plugins-bad1.0-dev
```

To see the blacklisted elements:
```
gst-inspect-1.0 -b
```

### Useful Links

[Gstreamer Tools](https://gstreamer.freedesktop.org/documentation/tutorials/basic/gstreamer-tools.html?gi-language=c)

[Table of Concepts](https://gstreamer.freedesktop.org/documentation/tutorials/table-of-concepts.html?gi-language=c)

[Nvidia Accelerated Gstreamer](https://docs.nvidia.com/jetson/archives/r35.6.2/DeveloperGuide/SD/Multimedia/AcceleratedGstreamer.html)