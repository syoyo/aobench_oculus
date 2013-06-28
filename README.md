# aobench for Oculus Rift

![aobench-oculus](https://github.com/syoyo/aobench_oculus/blob/master/screenshot/aobench_oculus.png?raw=true)

OpenGL GLSL version of aobench (https://code.google.com/p/aobench/) for Oculus Rift.
You can enjoy aobench in Virtual Reality!

## Platform

* MacOSX 10.8
* Windows(will be possible, but not yet tested)

## Requirements

* OpenGL 3.x(?)
* GLUT
* Oculus SDK
* premake4

## Build

    $ premake4 --oculus-sdk=/path/to/OculusSDK/LibOVR gmake
    $ make
  
## Run

    $ ./aobench_oculus

## Quality

Increase `N` value in `aobench.fs` for better aobench rendering.

## TODO

* Head-mount tracking.
* Better stereo rendering.
* Better anti-aliasing to compensate for chromatic aberration

## License

2-clause BSD. Except for `postprocess.fs`, which is licensed under Oculus SDK's apache 2.0 license.

`trackball.cc` and `trackball.h` are copyright/licensed by Silicon Graphics, Inc.
