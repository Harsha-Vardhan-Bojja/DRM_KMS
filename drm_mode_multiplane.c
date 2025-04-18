// TO KNOW HOW TO COMPLIE HAVE A LOOK AT README.md FILE

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

#define PRIMARY 1
#define OVERLAY 0
#define PROP_ID(obj, type, name) get_property_id(drm_fd, obj, type, name)

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
            printf("[CONNECTOR]    : ID = %d and STATUS = CONNECTED\n", conn->connector_id);
            printf("[MODE]         : %dx%d @%dHz\n", conn->modes->hdisplay, conn->modes->vdisplay, conn->modes->vrefresh);
            return 0;
        }

        drmModeFreeConnector(conn);
    }
    return -1;
}

// Pick a CRTC from the resource list (can be improved to match connector's encoder)
static int fetch_crtc(int drm_fd, drmModeRes *resources, drmModeConnector *connector, drmModeCrtc **crtc_out, int *index) {
    for (int i = 0; i < resources->count_crtcs; i++) {
        drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, resources->crtcs[i]);
        if (crtc) {
            *crtc_out = crtc;
            printf("[CRTC]         : ID = %d\n", crtc->crtc_id);
            *index = i;
            return 0;
        }
    }
    return -1;
}

// Find the primary plane associated with the selected CRTC
static int fetch_plane(int drm_fd, drmModeCrtc *crtc, drmModePlane **primary_plane, drmModePlane **overlay_plane, int *crtc_index) {
    drmModePlaneRes *planes = drmModeGetPlaneResources(drm_fd);
    if (!planes)
        return -1;

    for (uint32_t i = 0; i < planes->count_planes; i++) {
        drmModePlane *plane = drmModeGetPlane(drm_fd, planes->planes[i]);
        if (!plane)
            continue;

        int type = get_property_value(drm_fd, plane->plane_id, "type");
        if(plane->possible_crtcs & (1 << *crtc_index)) {
        if (type == DRM_PLANE_TYPE_PRIMARY) {
            printf("[PRIMARY_PLANE]: ID = %d and TYPE = PRIMARY\n", plane->plane_id);
            *primary_plane = plane;
            continue;
        }
        if (type == DRM_PLANE_TYPE_OVERLAY) {
            printf("[OVERLAY_PLANE]: ID = %d and TYPE = OVERLAY\n", plane->plane_id);
            *overlay_plane = plane;
            continue;
        }
        }
             drmModeFreePlane(plane); 
        }
    drmModeFreePlaneResources(planes);  
    return 0;
}

// Fill the framebuffer with a solid color (XRGB8888 format)
static int fill_color(uint8_t *data, int size, uint32_t color) {
    for (int i = 0; i < size; i += 4) {
        *(uint32_t *)(data + i) = color; // 4 bytes per pixel
    }
    return 0;
}

// Create a framebuffer using dumb buffer and map it to userspace memory
static int create_fb(int drm_fd, int *fb_id, int width, int height, int flag) {
   
    struct drm_mode_create_dumb create = {
        .height = height,
        .width = width,
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
    if (drmModeAddFB2(drm_fd, width, height, DRM_FORMAT_XRGB8888, handles, strides, offsets, fb_id, 0) != 0) {
        perror("drmModeAddFB2 failed");
        return -1;
    }

    if(flag)
      printf("[PRIMARY_FB]   : ID = %d\n", *fb_id);
    else
      printf("[OVERLAY_FB]   : ID = %d\n", *fb_id);
    // Map dumb buffer to userspace
    struct drm_mode_map_dumb map = {.handle = handle};
    if (drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        perror("DRM_IOCTL_MODE_MAP_DUMB failed");
        return -1;
    }

    uint8_t *data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, map.offset);
    if (data == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    uint32_t color;
    if(flag)
        color = 0xFF0000FF;
    else
        color = 0xFF00FF00;

    fill_color(data, size, color);

    return 0;
}

// Perform atomic commit to set plane, mode, and activate the display
int commit_fb(int drm_fd, drmModeConnector *connector, drmModeCrtc *crtc, drmModePlane *plane, drmModePlane *overlay_plane, int overlay_fb, int fb_id) {
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    if (!req) {
        fprintf(stderr, "Failed to allocate atomic request\n");
        return -1;
    }

    uint32_t blob_id = 0;
    drmModeCreatePropertyBlob(drm_fd, &crtc->mode, sizeof(crtc->mode), &blob_id);

    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID"), overlay_fb);
    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_ID"), crtc->crtc_id);
    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "SRC_X"), 0);
    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "SRC_Y"), 0 << 16);
    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "SRC_W"), crtc->mode.hdisplay / 4 << 16);
    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "SRC_H"), crtc->mode.vdisplay / 4 << 16);
    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_X"), 300);
    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_Y"), 400);
    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_W"), crtc->mode.hdisplay / 4);
    drmModeAtomicAddProperty(req, overlay_plane->plane_id, PROP_ID(overlay_plane->plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_H"), crtc->mode.vdisplay / 4);

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

    drmModeAtomicAddProperty(req, connector->connector_id, PROP_ID(connector->connector_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID"), crtc->crtc_id);
    drmModeAtomicAddProperty(req, crtc->crtc_id, PROP_ID(crtc->crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID"), blob_id);
    drmModeAtomicAddProperty(req, crtc->crtc_id, PROP_ID(crtc->crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE"), 1);

    int ret = drmModeAtomicCommit(drm_fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET | DRM_MODE_ATOMIC_NONBLOCK, NULL);
    if (ret < 0) perror("drmModeAtomicCommit failed");
    else printf("[ATOMIC]   : Commit successful\n");

    drmModeAtomicFree(req);
    return ret;
}

// Entry point
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
    drmModeConnector *connector = NULL;
    drmModeCrtc *crtc = NULL;
    drmModePlane *plane = NULL;
    drmModePlane *plane_1 = NULL;
    int crtc_index = 0;
    int fb_id = 0;
    int fb_id_1 = 0;
    int width = 1920;
    int height = 1080;

    
    // Full setup and commit
    if (fetch_connector(drm_fd, resources, &connector) == 0 &&
        fetch_crtc(drm_fd, resources, connector, &crtc, &crtc_index) == 0 &&
        fetch_plane(drm_fd, crtc, &plane, &plane_1,&crtc_index) == 0 &&
        create_fb(drm_fd, &fb_id, width, height, PRIMARY) == 0 &&
        create_fb(drm_fd, &fb_id_1, width/4,height/4, OVERLAY) == 0 ) {

        commit_fb(drm_fd, connector, crtc, plane, plane_1, fb_id_1, fb_id);
        drmModeFreePlane(plane);
        drmModeFreePlane(plane_1);
    }

    // Keep image on screen for 5 seconds
    sleep(5);

    // Cleanup
    drmModeFreeCrtc(crtc);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    close(drm_fd);
    return 0;
}
