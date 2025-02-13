#ifndef CAMERA_INIT_H
#define CAMERA_INIT_H

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_buffer.h>
#include <interface/mmal/mmal_port.h>
#include <interface/mmal/mmal_queue.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/mmal_parameters_camera.h>

/*
 * Simple struct to hold references to the camera component, output port,
 * buffer pool, and callback queue. This keeps everything in one place.
 */
typedef struct {
    MMAL_COMPONENT_T *camera;
    MMAL_PORT_T      *camera_output;
    MMAL_POOL_T      *pool;
    MMAL_QUEUE_T     *callback_queue;

    int width;
    int height;
    int framerate;
} CameraState;

// Helper: define mmal_port_parameter_set_boolean if not provided.
#ifndef mmal_port_parameter_set_boolean
static inline MMAL_STATUS_T mmal_port_parameter_set_boolean(
    MMAL_PORT_T *port, uint32_t parameter, MMAL_BOOL_T value)
{
    MMAL_PARAMETER_BOOLEAN_T param = {{parameter, sizeof(param)}, value};
    return mmal_port_parameter_set(port, &param.hdr);
}
#endif

/*
 * camera_init  : Initializes the camera for continuous capture on the VIDEO port.
 * camera_release: Cleans up all resources (disable component, destroy pool, etc.).
 *
 * Return 0 on success, or -1 on any failure.
 */
int camera_init(CameraState *state, int width, int height, int framerate);
void camera_release(CameraState *state);

#endif // CAMERA_INIT_H
