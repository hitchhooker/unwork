#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xcomposite.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#define JPEG_QUALITY 90

// Function to save an XImage to a JPEG file
void save_image_to_jpeg(XImage *image, const char *filename, int width, int height) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPROW row_pointer[1];
    int row_stride;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Unable to open file for writing\n");
        return;
    }

    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = width * 3;
    unsigned char *row_data = (unsigned char *)malloc(row_stride);

    while (cinfo.next_scanline < cinfo.image_height) {
        for (int x = 0; x < width; x++) {
            unsigned long pixel = XGetPixel(image, x, cinfo.next_scanline);
            row_data[x * 3] = (pixel & image->red_mask) >> 16;
            row_data[x * 3 + 1] = (pixel & image->green_mask) >> 8;
            row_data[x * 3 + 2] = pixel & image->blue_mask;
        }
        row_pointer[0] = row_data;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    free(row_data);
    jpeg_destroy_compress(&cinfo);
}

// Capture using XGetImage (basic method)
XImage* capture_xgetimage(Display *display, Window root, int width, int height) {
    return XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
}

// Capture using XShmGetImage (shared memory)
XImage* capture_xshm(Display *display, Window root, int width, int height) {
    XShmSegmentInfo shminfo;
    XImage *image = XShmCreateImage(display, DefaultVisual(display, 0), DefaultDepth(display, 0), ZPixmap, NULL, &shminfo, width, height);
    if (!image) return NULL;

    shminfo.shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT | 0777);
    if (shminfo.shmid < 0) return NULL;

    shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);
    shminfo.readOnly = False;

    if (!XShmAttach(display, &shminfo)) return NULL;

    XShmGetImage(display, root, image, 0, 0, AllPlanes);
    XShmDetach(display, &shminfo);
    shmdt(shminfo.shmaddr);
    shmctl(shminfo.shmid, IPC_RMID, 0);

    return image;
}

// Capture using XComposite
XImage* capture_xcomposite(Display *display, Window root, int width, int height) {
    XCompositeRedirectSubwindows(display, root, CompositeRedirectAutomatic);
    XImage *image = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
    XCompositeUnredirectSubwindows(display, root, CompositeRedirectAutomatic);
    return image;
}

// Capture using framebuffer (/dev/fb0)
int capture_framebuffer(const char *filename, int width, int height) {
    int fb_fd = open("/dev/fb0", O_RDONLY);
    if (fb_fd == -1) return -1;

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        close(fb_fd);
        return -1;
    }

    long screensize = vinfo.yres_virtual * vinfo.bits_per_pixel / 8;
    unsigned char *fb_data = (unsigned char *)malloc(screensize);
    pread(fb_fd, fb_data, screensize, 0);

    // Save framebuffer data as a simple PPM image
    FILE *f = fopen(filename, "wb");
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    fwrite(fb_data, screensize, 1, f);
    fclose(f);

    free(fb_data);
    close(fb_fd);
    return 0;
}

int main() {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open display\n");
        return 1;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    unsigned int width = DisplayWidth(display, screen);
    unsigned int height = DisplayHeight(display, screen);

    // Generate filename with timestamp for each method
    char filename[100];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    // A: XGetImage method
    strftime(filename, sizeof(filename), "shotter_A_%Y%m%d_%H%M%S.jpg", t);
    XImage *image = capture_xgetimage(display, root, width, height);
    if (image) save_image_to_jpeg(image, filename, width, height);

    // B: XShmGetImage method
    // strftime(filename, sizeof(filename), "shotter_B_%Y%m%d_%H%M%S.jpg", t);
    // image = capture_xshm(display, root, width, height);
    // if (image) save_image_to_jpeg(image, filename, width, height);

    // C: XComposite method
    strftime(filename, sizeof(filename), "shotter_C_%Y%m%d_%H%M%S.jpg", t);
    image = capture_xcomposite(display, root, width, height);
    if (image) save_image_to_jpeg(image, filename, width, height);

    // D: Framebuffer capture
    strftime(filename, sizeof(filename), "shotter_D_%Y%m%d_%H%M%S.ppm", t);
    capture_framebuffer(filename, width, height);

    XCloseDisplay(display);
    return 0;
}
