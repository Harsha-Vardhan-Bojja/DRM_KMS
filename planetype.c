/*
 * DRM/KMS Display Info Printer + Plane Type Detector
 * ----------------------------------------------------
 * This program queries and prints DRM/KMS display information,
 * including CRTCs, connectors, and plane types (Primary, Overlay, Cursor).
 *
 * Compile with:
 *     gcc planetype.c -o planetype -ldrm -I/usr/include/libdrm
 *
 * Run with:
 *     sudo ./planetype or ./planetype
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h> 
#include <xf86drm.h>
#include <xf86drmMode.h>

int main() {
    // Open DRM device node (card1 can vary - use card0 if needed)
    int drm_fd = open("/dev/dri/card1", O_RDWR | O_NONBLOCK);
    if (drm_fd < 0) {
        printf("Failed to open GPU /dev/dri/card1\n");
        return -1;
    }

    if (drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0) {
      printf("drmSetClientCap(UNIVERSAL_PLANES) failed");
      return 1;
	  }
	  
    // Get basic resources (CRTCS, connectors, encoders, etc.)
    drmModeRes *resources = drmModeGetResources(drm_fd);
    if (!resources) {
        printf("Failed to get DRM resources.\n");
        close(drm_fd);
        return -1;
    }

    printf("********************************************************************************\n");
    printf("NOTE: DRM resources do not provide framebuffer info (count_fbs is usually 0).\n");
    printf("Total no.of crtc's        = %d\n", resources->count_crtcs);
    printf("Total no.of connector's   = %d\n", resources->count_connectors);
    printf("Total no.of encoder's     = %d\n", resources->count_encoders);
    printf("Max_Width = %d and Max_height = %d\n", resources->max_width, resources->max_height);
    printf("Min_Width = %d and Min_height = %d\n", resources->min_width, resources->min_height);
    printf("********************************************************************************\n");

    // List connected display modes
    for (int i = 0; i < resources->count_connectors; i++) {
        uint32_t conn_id = resources->connectors[i];
        drmModeConnector *conn = drmModeGetConnector(drm_fd, conn_id);
        if (!conn || conn->connection != DRM_MODE_CONNECTED) {
            drmModeFreeConnector(conn);
            continue;
        }

        printf("Modes for connector %u:\n", conn->connector_id);
        for (int j = 0; j < conn->count_modes; j++) {
            drmModeModeInfo *mode = &conn->modes[j];
            printf("%s: %dx%d %dHz\n", mode->name, mode->hdisplay, mode->vdisplay, mode->vrefresh);
        }

        drmModeFreeConnector(conn);
    }

    // Get all plane resources
    drmModePlaneRes *plane_res = drmModeGetPlaneResources(drm_fd);
    if (!plane_res) {
        printf("Failed to get plane resources.\n");
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    // Iterate through each plane to detect its type
    for (int i = 0; i < plane_res->count_planes; i++) {
        uint32_t plane_id = plane_res->planes[i];
        drmModePlane *plane = drmModeGetPlane(drm_fd, plane_id);
        if (!plane)
            continue;

        // Get properties of the current plane
        drmModeObjectProperties *props = drmModeObjectGetProperties(drm_fd, plane_id, DRM_MODE_OBJECT_PLANE);
        if (!props) {
            drmModeFreePlane(plane);
            continue;
        }

        const char *type_str = "Unknown";

        // Check each property to find the "type"
        for (int j = 0; j < props->count_props; j++) {
            uint32_t prop_id = props->props[j];
            drmModePropertyPtr property = drmModeGetProperty(drm_fd, prop_id);
            if (!property) continue;

            if (strcmp(property->name, "type") == 0) {
                uint64_t type = props->prop_values[j];
                if (type == DRM_PLANE_TYPE_PRIMARY)
                    type_str = "Primary";
                else if (type == DRM_PLANE_TYPE_OVERLAY)
                    type_str = "Overlay";
                else if (type == DRM_PLANE_TYPE_CURSOR)
                    type_str = "Cursor";
            }

            drmModeFreeProperty(property);
        }

        printf("Plane ID %u - Type: %s\n", plane_id, type_str);

        drmModeFreeObjectProperties(props);
        drmModeFreePlane(plane);
    }

    // Cleanup
    drmModeFreePlaneResources(plane_res);
    drmModeFreeResources(resources);
    close(drm_fd);
    return 0;
}
