#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <time.h>

#define JPEG_QUALITY 90

int main() {
    Display *display;
    Window root;
    XImage *image;
    int screen;
    unsigned int width, height;

    // Open the display
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open display\n");
        return 1;
    }

    // Get the root window
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    // Get screen dimensions
    width = DisplayWidth(display, screen);
    height = DisplayHeight(display, screen);

    printf("Detected screen size: %ux%u\n", width, height);

    // Get the image of the root window (full screen)
    image = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
    if (image == NULL) {
        fprintf(stderr, "Unable to get image\n");
        XCloseDisplay(display);
        return 1;
    }

    // Prepare the JPEG compression
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPROW row_pointer[1];
    int row_stride;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // Generate filename with timestamp
    char filename[100];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(filename, sizeof(filename), "shotter_%Y%m%d_%H%M%S.jpg", t);

    // Open the output file
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Unable to open file for writing\n");
        XDestroyImage(image);
        XCloseDisplay(display);
        return 1;
    }

    jpeg_stdio_dest(&cinfo, outfile);

    // Set image parameters
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    // Set JPEG compression parameters
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE);

    // Start compression
    jpeg_start_compress(&cinfo, TRUE);

    // Allocate memory for one row
    row_stride = width * 3;
    unsigned char *row_data = (unsigned char *)malloc(row_stride);

    // Compress the image
    while (cinfo.next_scanline < cinfo.image_height) {
        for (int x = 0; x < width; x++) {
            unsigned long pixel = XGetPixel(image, x, cinfo.next_scanline);
            row_data[x*3]   = (pixel & image->red_mask) >> 16;
            row_data[x*3+1] = (pixel & image->green_mask) >> 8;
            row_data[x*3+2] = pixel & image->blue_mask;
        }
        row_pointer[0] = row_data;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    // Finish compression
    jpeg_finish_compress(&cinfo);
    fclose(outfile);

    // Clean up
    free(row_data);
    jpeg_destroy_compress(&cinfo);
    XDestroyImage(image);
    XCloseDisplay(display);

    printf("Screenshot saved as %s\n", filename);
    return 0;
}
