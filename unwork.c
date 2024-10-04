#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <pthread.h>

#define BLUR_RADIUS 5
#define NUM_THREADS 4
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef XImage* (*orig_XGetImage_t)(Display*, Drawable, int, int, unsigned int, unsigned int, unsigned long, int);

typedef struct {
    XImage *image;
    unsigned long *temp;
    int start_y;
    int end_y;
    int radius;
} BlurThreadData;

static void* blur_thread(void *arg) {
    BlurThreadData *data = (BlurThreadData*)arg;
    XImage *image = data->image;
    unsigned long *temp = data->temp;
    const int width = image->width;
    const int height = image->height;
    const int radius = data->radius;

    for (int y = data->start_y; y < data->end_y; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t r = 0, g = 0, b = 0;
            int count = 0;
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        unsigned long pixel = XGetPixel(image, nx, ny);
                        r += (pixel >> 16) & 0xFF;
                        g += (pixel >> 8) & 0xFF;
                        b += pixel & 0xFF;
                        count++;
                    }
                }
            }
            temp[y * width + x] = ((r / count) << 16) | ((g / count) << 8) | (b / count);
        }
    }
    return NULL;
}

static void apply_blur(XImage *image, int radius) {
    const int width = image->width;
    const int height = image->height;
    unsigned long *temp = malloc(width * height * sizeof(unsigned long));
    if (!temp) {
        fprintf(stderr, "Failed to allocate memory for blur operation\n");
        return;
    }

    pthread_t threads[NUM_THREADS];
    BlurThreadData thread_data[NUM_THREADS];
    int rows_per_thread = height / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].image = image;
        thread_data[i].temp = temp;
        thread_data[i].start_y = i * rows_per_thread;
        thread_data[i].end_y = (i == NUM_THREADS - 1) ? height : (i + 1) * rows_per_thread;
        thread_data[i].radius = radius;
        pthread_create(&threads[i], NULL, blur_thread, &thread_data[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            XPutPixel(image, x, y, temp[y * width + x]);
        }
    }

    free(temp);
}

XImage* XGetImage(Display *display, Drawable drawable, int x, int y, unsigned int width, unsigned int height, unsigned long plane_mask, int format) {
    static orig_XGetImage_t orig_XGetImage = NULL;
    if (!orig_XGetImage) {
        orig_XGetImage = (orig_XGetImage_t) dlsym(RTLD_NEXT, "XGetImage");
        if (!orig_XGetImage) {
            fprintf(stderr, "Failed to find original XGetImage function: %s\n", dlerror());
            return NULL;
        }
    }

    XImage *original_image = orig_XGetImage(display, drawable, x, y, width, height, plane_mask, format);
    if (!original_image) {
        fprintf(stderr, "Failed to get original image.\n");
        return NULL;
    }

    apply_blur(original_image, BLUR_RADIUS);
    return original_image;
}

__attribute__((constructor))
static void init(void) {
    fprintf(stderr, "Blur effect initialized\n");
}

__attribute__((destructor))
static void cleanup(void) {
    fprintf(stderr, "Blur effect cleaned up\n");
}
