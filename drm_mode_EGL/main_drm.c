#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#include <inttypes.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include "cube_render.h"

// Helper function to get the *value* of a property by name for a given plane
static int get_property_value(int drm_fd, uint32_t plane_id, const char *name) {
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(drm_fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!props)
        return -1;

    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(drm_fd, props->props[i]);
        if (!prop)
            continue;

        if (strcmp(prop->name, name) == 0) {
            uint64_t value = props->prop_values[i];
            drmModeFreeProperty(prop);
            drmModeFreeObjectProperties(props);
            return (int)value;
        }

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);
    return -1;
}

// Helper function to get the *ID* of a property by name for a given DRM object
static uint32_t get_property_id(int drm_fd, uint32_t obj_id, uint32_t obj_type, const char *name) {
    drmModeObjectProperties *props = drmModeObjectGetProperties(drm_fd, obj_id, obj_type);
    if (!props)
        return 0;

    uint32_t prop_id = 0;
    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(drm_fd, props->props[i]);
        if (!prop)
            continue;

        if (strcmp(prop->name, name) == 0)
            prop_id = prop->prop_id;

        drmModeFreeProperty(prop);
        if (prop_id)
            break;
    }

    drmModeFreeObjectProperties(props);
    return prop_id;
}

// Find the first connected connector with a valid mode
static int fetch_connector(int drm_fd, drmModeRes *resources, drmModeConnector **connector_out) {
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(drm_fd, resources->connectors[i]);
        if (!conn)
            continue;

        if ((conn->connection == DRM_MODE_CONNECTED) && (conn->modes != NULL)) {
            *connector_out = conn;
            printf("[CONNECTOR]: ID = %d and STATUS = CONNECTED\n", conn->connector_id);
            printf("[MODE]     : %dx%d @%dHz\n", conn->modes->hdisplay, conn->modes->vdisplay, conn->modes->vrefresh);
            return 0;
        }

        drmModeFreeConnector(conn);
    }
    return -1;
}

// Pick a CRTC from the resource list (can be improved to match connector's encoder)
static int fetch_crtc(int drm_fd, drmModeRes *resources, drmModeConnector *connector, drmModeCrtc **crtc_out, int *crtc_indx) {
    for (int i = 0; i < resources->count_crtcs; i++) {
        drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, resources->crtcs[i]);
        if (crtc) {
            *crtc_out = crtc;
            printf("[CRTC]     : ID = %d\n", crtc->crtc_id);
            *crtc_indx = i;
            return 0;
        }
    }
    return -1;
}

// Find the primary plane associated with the selected CRTC
static int fetch_plane(int drm_fd, drmModeCrtc *crtc, drmModePlane **plane_out, int crtc_indx) {
    drmModePlaneRes *planes = drmModeGetPlaneResources(drm_fd);
    if (!planes)
        return -1;

    for (uint32_t i = 0; i < planes->count_planes; i++) {
        drmModePlane *plane = drmModeGetPlane(drm_fd, planes->planes[i]);
        if (!plane)
            continue;

        if (plane->possible_crtcs & (1 << crtc_indx)) {
            int type = get_property_value(drm_fd, plane->plane_id, "type");
            if (type == DRM_PLANE_TYPE_PRIMARY) {
                printf("[PLANE]    : ID = %d and TYPE = PRIMARY\n", plane->plane_id);
                *plane_out = plane;
                drmModeFreePlaneResources(planes);
                return 0;
            }
        }

        drmModeFreePlane(plane);
    }

    drmModeFreePlaneResources(planes);  
    return -1;
}

// Fill the framebuffer with a solid color (XRGB8888 format)
static int fill_color(uint8_t *data, int size, uint32_t color) {
    for (int i = 0; i < size; i += 4) {
        *(uint32_t *)(data + i) = color; // 4 bytes per pixel
    }
    return 0;
}

// Create a framebuffer using dumb buffer and map it to userspace memory
static int create_fb(int drm_fd, drmModeCrtc *crtc, int *fb_id, uint8_t **out_Address) {
    int width = crtc->mode.hdisplay;
    int height = crtc->mode.vdisplay;

    struct drm_mode_create_dumb create = {
        .height = (uint32_t)height,
        .width = (uint32_t)width,
        .bpp = 32 // 4 bytes per pixel (XRGB8888)
    };

    // Create dumb buffer
    if (drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        perror("DRM_IOCTL_MODE_CREATE_DUMB failed");
        return -1;
    }

    uint32_t handle = create.handle;
    uint32_t stride = create.pitch;
    uint32_t size = create.size;

    uint32_t handles[4] = {handle};
    uint32_t strides[4] = {stride};
    uint32_t offsets[4] = {0};

    // Add framebuffer
    if (drmModeAddFB2(drm_fd, width, height, DRM_FORMAT_XRGB8888, handles, strides, offsets, (uint32_t*)fb_id, 0) != 0) {
        perror("drmModeAddFB2 failed");
        return -1;
    }

    printf("[FB]       : ID = %d\n", *fb_id);

    // Map dumb buffer to userspace
    struct drm_mode_map_dumb map = {.handle = handle};
    if (drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        perror("DRM_IOCTL_MODE_MAP_DUMB failed");
        return -1;
    }

    void *data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, map.offset);
    if (data == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    // Fill with blue (XRGB: 0xFF0000FF)
    uint32_t color = 0xFF0000FF;
    fill_color((uint8_t*)data, size, color);

    *out_Address = (uint8_t *)data;
    return 0;
}

// Perform atomic commit to set plane, mode, and activate the display
int commit_fb(int drm_fd, drmModeConnector *connector, drmModeCrtc *crtc, drmModePlane *plane, int fb_id) {
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    if (!req) {
        fprintf(stderr, "Failed to allocate atomic request\n");
        return -1;
    }

    #define PROP_ID(obj, type, name) get_property_id(drm_fd, obj, type, name)

    // Create a MODE_ID blob from crtc mode
    drmModePropertyBlobPtr mode_blob;
    uint32_t blob_id = 0;
    drmModeCreatePropertyBlob(drm_fd, &crtc->mode, sizeof(crtc->mode), &blob_id);

    // Plane setup
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID"), fb_id);
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_ID"), crtc->crtc_id);
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "SRC_X"), 0);
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "SRC_Y"), 0);
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "SRC_W"), crtc->mode.hdisplay << 16);
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "SRC_H"), crtc->mode.vdisplay << 16);
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_X"), 0);
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_Y"), 0);
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_W"), crtc->mode.hdisplay);
    drmModeAtomicAddProperty(req, plane->plane_id, PROP_ID(plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_H"), crtc->mode.vdisplay);

    // Connector + CRTC setup
    drmModeAtomicAddProperty(req, connector->connector_id, PROP_ID(connector->connector_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID"), crtc->crtc_id);
    drmModeAtomicAddProperty(req, crtc->crtc_id, PROP_ID(crtc->crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID"), blob_id);
    drmModeAtomicAddProperty(req, crtc->crtc_id, PROP_ID(crtc->crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE"), 1);

    // Do the commit
    int ret = drmModeAtomicCommit(drm_fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET | DRM_MODE_ATOMIC_NONBLOCK, NULL);
    if (ret < 0) {
        perror("drmModeAtomicCommit failed");
    } else {
        printf("[ATOMIC]   : Commit successful\n");
    }

    drmModeAtomicFree(req);
    return ret;
}

int main() {
    // Open DRM device
    int drm_fd = open("/dev/dri/card1", O_RDWR | O_NONBLOCK);
    if (drm_fd < 0) {
        perror("Failed to open DRM device");
        return -1;
    }

    // Enable atomic + universal planes
    drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmSetClientCap(drm_fd, DRM_CLIENT_CAP_ATOMIC, 1);

    // Get DRM resources
    drmModeRes *resources = drmModeGetResources(drm_fd);
    if (!resources) {
        perror("Failed to get DRM resources");
        close(drm_fd);
        return -1;
    }

    drmModeConnector *connector = NULL;
    drmModeCrtc *crtc = NULL;
    drmModePlane *plane = NULL;
    uint32_t fb_id = 0;
    uint8_t *dumb_buffer_data = NULL;
    int width = 0;
    int height = 0;
    int crtc_indx;
    
    if (fetch_connector(drm_fd, resources, &connector) != 0) {
        fprintf(stderr, "Failed to find connector\n");
        goto cleanup;
    }

    if (fetch_crtc(drm_fd, resources, connector, &crtc, &crtc_indx) != 0) {
        fprintf(stderr, "Failed to find CRTC\n");
        goto cleanup;
    }

    if (fetch_plane(drm_fd, crtc, &plane, crtc_indx) != 0) {
        fprintf(stderr, "Failed to find plane\n");
        goto cleanup;
    }

    
    width = connector->modes[0].hdisplay;
    height = connector->modes[0].vdisplay;
   
    if (create_fb(drm_fd, crtc, &fb_id, &dumb_buffer_data) != 0) {
        fprintf(stderr, "Failed to create framebuffer\n");
        goto cleanup;
    }

    // Initialize EGL and OpenGL
    if (EGL_init(width, height) < 0) {
        fprintf(stderr, "Failed to initialize EGL\n");
        goto cleanup;
    }

    // Set up textures and framebuffers once
    if (setup_textures_framebuffers(width, height) < 0) {
        fprintf(stderr, "Failed to setup textures and framebuffers\n");
        goto cleanup;
    }

    // Perform initial atomic commit to set mode 
    if (commit_fb(drm_fd, connector, crtc, plane, fb_id) < 0) {
        fprintf(stderr, "Initial atomic commit failed\n");
        goto cleanup;
    }

    // Main render loop
    clock_t start_time = clock();
    int frame_count = 1000;
    
    for (int i = 0; i < frame_count; i++) {
        clock_t frame_start = clock();
        
        // Render the cube
        render_the_cube(width, height, dumb_buffer_data);
        
        // Perform atomic commit
        if (commit_fb(drm_fd, connector, crtc, plane, fb_id) < 0) {
            fprintf(stderr, "Frame %d: Atomic commit failed\n", i);
            break;
        }
        
        // Calculate frame time
        clock_t frame_end = clock();
        double frame_time = (double)(frame_end - frame_start) / CLOCKS_PER_SEC;
        double fps = 1.0 / frame_time;
        
        printf("Frame %d: Time = %.3f ms, FPS = %.2f\n", i + 1, frame_time * 1000.0, fps);
    }

    // Calculate total time
    clock_t end_time = clock();
    double total_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Total time for rendering %d frames: %.2f seconds\n", frame_count, total_time);
    printf("Average FPS: %.2f\n", frame_count / total_time);

cleanup:
    // Cleanup resources
    cleanup_gl_setup();
    
    drmModeFreePlane(plane);
    drmModeFreeCrtc(crtc);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);

    if (fb_id) {
        struct drm_mode_destroy_dumb destroy = {0};
        if (dumb_buffer_data) {
            munmap(dumb_buffer_data, width * height * 4);
        }
        if (drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy) < 0) {
            perror("Failed to destroy dumb buffer");
        }
        drmModeRmFB(drm_fd, fb_id);
    }
    
    close(drm_fd);
    return 0;
}
