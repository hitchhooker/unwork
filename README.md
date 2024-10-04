# unwork

`unwork` is a dynamic library designed to intercept and modify X11 screenshot operations, applying a blur effect to captured images. `shotter` is a simple screenshot utility used for testing `unwork`.

## Features

- `unwork`: Intercepts X11 `XGetImage` calls and applies a configurable blur effect
- `shotter`: Captures full-screen screenshots and saves them as JPEG files

## Prerequisites

To build and use these tools, you need the following:

- GCC (GNU Compiler Collection)
- X11 development files
- pthread library (usually included with GCC)
- libjpeg development files (for `shotter`)

On Ubuntu or Debian-based systems, you can install these with:

```bash
sudo apt-get install gcc libx11-dev libjpeg-dev
```

## Building

To compile `unwork`, run the following command:

```bash
gcc -O3 -march=native -ffast-math -pthread -Wall -Wextra -fPIC -shared -o unwork.so unwork.c -ldl -lX11
```

This will create a shared library named `unwork.so`.

To compile `shotter`, run:

```bash
gcc -o shotter shotter.c -lX11 -lXext -lXcomposite -ljpeg
```

This will create an executable named `shotter`.

## Usage

To use `unwork` with any X11 application that takes screenshots, including `shotter`, you can preload it using the `LD_PRELOAD` environment variable:

```bash
LD_PRELOAD=/path/to/unwork.so your_screenshot_application
```

For example, to use it with `shotter`:

```bash
LD_PRELOAD=/path/to/unwork.so ./shotter
```

To use `shotter` without `unwork`, simply run:

```bash
./shotter
```

## Configuration

For `unwork`:
- The blur radius can be adjusted by modifying the `BLUR_RADIUS` define in `unwork.c`.
- The number of threads used for processing can be adjusted by modifying the `NUM_THREADS` define.

For `shotter`:
- The JPEG quality can be adjusted by modifying the `JPEG_QUALITY` define in `shotter.c`.

## How It Works

`unwork` uses the `LD_PRELOAD` mechanism to intercept calls to the `XGetImage` function. When a screenshot is taken, `unwork` applies a multi-threaded box blur to the image before returning it to the calling application.

`shotter` uses X11 functions to capture a full-screen screenshot and libjpeg to save it as a JPEG file with a timestamp in the filename.
