# prgsem
SDL applicalion and compute module for computing and displaying fractals. Seminary work for B3B36PRG at CTU Prague.

## Build
- Install prerequirements (package names from Ubuntu repositories):
```sudo apt update && sudo apt install -y build-essential gcc make git libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libpthread-stubs0-dev libpng-dev libjpeg-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev pkg-config git```
- (```libpng-dev libjpeg-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev``` only if you would like to build CLI features)
- ```make``` to make full application
- ```make ENABLE_CLI=1 ENABLE_HANDSHAKE=0``` to enable/disable built of components
- ```make run-mkpipes``` shortcut to call script to prepare named pipes

This will produce these binaries in ```build``` directory:
- ```prgsem-module``` (computational module)
- ```prgsem-main``` (graphical application that controlls it)

## Controlling graphical application
### Key binding
App catches events from both window and console, so usage of both is possible.
- ```q``` - quit
- ```a``` - abort
- ```r``` - reset chunk id
- ```g``` - get version
- ```s``` - switching between resolutions
- ```l``` - clear buffer
- ```p``` - redraw window
- ```c``` - compute locally
- ```1``` - compute using module
- ```h``` - show help screen
- ```i/o``` - zoom in/out into bounding box
- ```arrows``` - move bounding box in each direction
- ```b``` - print currect state information (useful in image/video generation)
Note: after changing bounding box, local compute is called for automatic preview. To compute using module you must manually press ```1```.

## CLI-only subsystem control
CLI only subsystem is enabled using ```--cli``` flag and allows us to create images or videos of fractals defined by arguments.
### Generating single image:
Set bounding box and computation parameters and add ```--cli``` and ```--output FILENAME```:
```
./build/prgsem-main \
    --cli \
    --c-re -0.4 \
    --c-im 0.6 \
    --n 100 \
    --range-re -1.6 1.6 \
    --range-im -1.0 1.0 \
    -w 1200 \
    -h 900 \
    --output output.png
```
Only ```.png``` and ```.jpg``` file types are currently supported.

## Generating zoom animation:
```
./build/prgsem-main \
    --cli \
    --width 640 \
    --height 480 \
    --c-re -0.4 \
    --c-im 0.6 \
    --n 60 \
    --range-re -0.2 0.2 \
    --range-im -0.2475 0.0275 \
    --anim-fps 30 \
    --anim-duration 10 \
    --anim-zoom 0.2 \
    --output output.mp4
```
Where ```--anim-zoom``` works the same way as i/o in graphical mode: > 1 zooms out and (0, 1) zooms in. This is how much it will zoom during ```--anim_duration```.
Currently only .mp4 video format is supported and for generating, these parameters are used for best speed/quality/usability ratio of our fractal images:
```
include/ffmpeg_writer.h:
#define VIDEO_BITRATE 10 * 1000 * 1000; // 10 Mbps
#define VIDEO_CFR "18" // 0 = lossless, 18 = visually lossless
#define VIDEO_CODEC AV_CODEC_ID_H264
```

## Implemented optional features
- catching window events, preventing it from freezing
- automatic versioning from git in Makefile, version compatibility checking
- handshake with compute module, compatibility testing
- interactive moving, zooming bounding box
- cli tool, static image and video generating
