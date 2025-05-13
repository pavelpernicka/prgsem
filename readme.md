# prgsem
SDL applicalion and compute module for computing and displaying fractals. Seminary work for B3B36PRG at CTU Prague.

## Build
- Install prerequirements:
```sudo apt update && sudo apt install -y build-essential gcc make git libsdl2-dev libsdl2-image-dev libpthread-stubs0-dev pkg-config git```
- ```make```

This will produce these binaries in build directory:
- ```prgsem-module``` (computational module)
- ```prgsem-main``` (graphical application that controlls it)

## Controlling graphical application
App catches events from both window and console, so usage of both is possible.
- ```q``` - quit
- ```a``` - abort
- ```r``` - reset chunk id
- ```g``` - get version
- ```s``` - switching between resolutions
- ```l``` - clear buffer
- ```p``` - redraw window
- ```c``` - compute locally

## Implemented optional features
- catching window events, preventing it from freezing
- automatic versioning from git in Makefile, version compatibility checking
