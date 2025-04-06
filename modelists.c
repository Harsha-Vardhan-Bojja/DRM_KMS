/*
 * DRM/KMS Display Info Printer
 * --------------------------------------------
 * This program queries and prints basic DRM/KMS display information,
 * such as number of CRTCs, connectors, encoders, and available modes.
 *
 * Compile with:
 *     gcc modelists.c -o modelists -ldrm -I/usr/include/libdrm
 *
 * Run with (may need sudo depending on access to /dev/dri):
 *     ./modelists
 */

#include <stdio.h> 
#include <fcntl.h>
#include <stdint.h>
#include <xf86drmMode.h>        // For DRM/KMS structures and functions

int main() {
    // Open the DRM device (/dev/dri/card1) in read-write and non-blocking mode
    int drm_fd = open("/dev/dri/card1", O_RDWR | O_NONBLOCK);
    if (drm_fd < 0) {
        printf("Failed to open GPU Card1\n");
        return -1;
    }

    // Get the DRM device resources (CRTCS, connectors, encoders, etc.)
    drmModeRes *resources = drmModeGetResources(drm_fd);

    // Print basic information from the resources
    printf("********************************************************************************\n");
    printf("NOTE: DRM resources do not provide framebuffer info (count_fbs is usually 0).\n");
    printf("Total no.of crtc's        = %d\n", resources->count_crtcs);
    printf("Total no.of connector's   = %d\n", resources->count_connectors);
    printf("Total no.of encoder's     = %d\n", resources->count_encoders);
    printf("Max_Width = %d and Max_height = %d\n", resources->max_width, resources->max_height);
    printf("Min_Width = %d and Min_height = %d\n", resources->min_width, resources->min_height);
    printf("********************************************************************************\n");

    // Loop over all connectors to find connected displays and their supported modes
    for (int i = 0; i < resources->count_connectors; i++) {
        uint32_t conn_id = resources->connectors[i];

        // Get connector info (e.g., HDMI, DisplayPort, etc.)
        drmModeConnector *conn = drmModeGetConnector(drm_fd, conn_id);
        if (conn->connection != DRM_MODE_CONNECTED) {
            // Skip if this connector is not currently connected
            drmModeFreeConnector(conn);
            continue;
        }

        // Print mode info for this connector
        printf("Modes for connector %u:\n", conn->connector_id);
        for (int i = 0; i < conn->count_modes; i++) {
            drmModeModeInfo *mode = &conn->modes[i];
            printf("%s: %dx%d %dHz\n", mode->name ,mode->hdisplay, mode->vdisplay, mode->vrefresh);
        }

        drmModeFreeConnector(conn); // Free connector data after use
    }

    drmModeFreeResources(resources); // Free main DRM resources
    return 0;
}
