#ifndef CUBE_RENDER_H
#define CUBE_RENDER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdint.h>

// Function pointer type for the extension
typedef EGLDisplay (EGLAPIENTRY *PFNEGLGETPLATFORMDISPLAYEXTPROC)(EGLenum platform, void *native_display, const EGLint *attrib_list);

// EGL context structure
struct {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
} egl;

#ifdef __cplusplus
extern "C" {
#endif

int EGL_init(int width, int height);
int render_the_cube(int width, int height, uint8_t* dumb_buffer);
int setup_textures_framebuffers(int width, int height);
int cleanup_gl_setup();

#ifdef __cplusplus
}
#endif

#endif // CUBE_RENDER_H