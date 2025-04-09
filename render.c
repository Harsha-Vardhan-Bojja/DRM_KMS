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


static int fetch_connector(int drm_fd, drmModeRes *resources, drmModeConnector *connector) {
    for(int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(drm_fd, resources->connectors[i]);
        if((connector->connection == DRM_MODE_CONNECTED )&& (connector->modes != NULL)) {
            printf("[CONNECTOR]: Connector_id = %d and Connected_type = %d\n", connector->connector_id, connector->connector_type);
            printf("[MODE]: Using %dx%d %d\n", connector->modes->hdisplay,connector->modes->vdisplay,connector->modes->vrefresh);
            return 0;
        }
    }
    return -1;
}

static int fetch_crtc(int drm_fd, drmModeRes *resources, drmModeConnector *connector, drmModeCrtc *crtc) {
    for(int i = 0; i < resources->count_crtcs; i++) {
        crtc = drmModeGetCrtc(drm_fd, resources->crtcs[i]);
        if(crtc) {
            printf("[CRTC]: Crtc_id = %d\n", crtc->crtc_id);
            return 0;
        }
    }
    return -1;
}

static int fetch_plane(int drm_fd, drmModeCrtc *crtc, drmModePlane **plane_out) {
    drmModePlaneRes *planes = drmModeGetPlaneResources(drm_fd);
    if (!planes)
        return -1;

    for (uint32_t i = 0; i < planes->count_planes; i++) {
        uint32_t plane_id = planes->planes[i];
        drmModePlane *tmp_plane = drmModeGetPlane(drm_fd, plane_id);
        if (!tmp_plane)
            continue;

        if (tmp_plane->crtc_id == crtc->crtc_id) {
            int type = get_property_value(drm_fd, plane_id, "type");
            if (type == DRM_PLANE_TYPE_PRIMARY) {
                printf("[PLANE]: plane_id = %d and type = Primary\n", plane_id);
                *plane_out = tmp_plane;
                drmModeFreePlaneResources(planes);
                return 0;
            }
        }

        drmModeFreePlane(tmp_plane);
    }

    drmModeFreePlaneResources(planes);
    return -1;
}

int main() {


    drmModeConnector *connector = NULL;
    drmModeCrtc *crtc = NULL;
    drmModePlane *plane = NULL;
    int ret = -1;
    
    // Open DRM device node (card1 can vary - use card0 if needed)
    int drm_fd = open("/dev/dri/card1", O_RDWR | O_NONBLOCK);
    if (drm_fd < 0) {
        printf("Failed to open GPU /dev/dri/card1\n");
        return -1;
    }

    if (drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0) {
        printf("drmSetClientCap(UNIVERSAL_PLANES) failed");
        return -1;
	  }

    if (drmSetClientCap(drm_fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0) {
        printf("drmSetClientCap(ATOMIC) failed");
        return -1;
	  }

    // Get basic resources (CRTCS, connectors, encoders, etc.)
    drmModeRes *resources = drmModeGetResources(drm_fd);
    if (!resources) {
        printf("Failed to get DRM resources.\n");
        close(drm_fd);
        return -1;
    }

    ret = fetch_connector(drm_fd, resources, connector);
    if(ret < 0) {
        printf("Failed to Fetch the Connector\n");
        return -1;
    }

    ret = fetch_crtc(drm_fd, resources, connector, crtc);
    if(ret < 0) {
        printf("Failed to fetch the crtc\n");
        return -1;
    }

    if (fetch_plane(drm_fd, crtc, &plane) == 0) {
    // use the plane
    printf("Plane ID = %d\n", plane->plane_id);
    drmModeFreePlane(plane);
    } else {
        fprintf(stderr, "Primary plane not found\n");
    }

    //ret = create_fb();
    



    return 0;
}