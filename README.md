# AX-MP4
Minimal MP4 demuxer for Cinder

## 15/01/2026
Simple MP4 parser to pull frames out of a container file as well as a sample application demonstrating pulling HAP and MJPEG frames out and rendering them.

Check out recursively into `{your_cinder_path}/blocks/` folder. HAP and Snappy are included as submodules and are required to build the sample.

```
$ cd ${your_cinder_path}/blocks && git clone --recursive https://github.com/axjxwright/AX-MP4
$ cd AX-MP4/samples/SimpleHAPDecoderApp
$ mkdir build && cd build
$ cmake ..\proj\cmake
$ start SimpleHAPDecoderApp.sln
```

This is a far from exhaustive implementation, only the atoms I needed to get the demo working have been implemented, but it's trivial to add new ones as they become necessary.

Lots of inspiration drawn from [ISOBMFF](https://github.com/DigiDNA/ISOBMFF) during development.