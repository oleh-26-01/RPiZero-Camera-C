#include "camera_init.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Callback that enqueues filled buffers into the user queue. We'll
 * set this as the camera output port's callback function.
 */
static void camera_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    MMAL_QUEUE_T *queue = (MMAL_QUEUE_T *)port->userdata;
    if (queue) {
        mmal_queue_put(queue, buffer);
    } else {
        mmal_buffer_header_release(buffer);
    }
}

/*
 * Initialize the camera for capturing frames at (width x height) in RGB24,
 * using the video port at the given framerate.
 */
int camera_init(CameraState *state, int width, int height, int framerate)
{
    bcm_host_init();

    // Zero out the struct just to be safe.
    memset(state, 0, sizeof(*state));
    state->width      = width;
    state->height     = height;
    state->framerate  = framerate;

    // Create the camera component.
    MMAL_STATUS_T status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &state->camera);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "Failed to create camera component.\n");
        return -1;
    }

    // Choose the video port (port 1) if available, or port 0 otherwise.
    MMAL_PORT_T *video_port = NULL;
    if (state->camera->output_num >= 2) {
        video_port = state->camera->output[1];  // video port
    } else {
        video_port = state->camera->output[0];  // fallback
    }
    state->camera_output = video_port;

    // Configure the video port for RGB24 @ state->framerate.
    MMAL_ES_FORMAT_T *format = video_port->format;
    format->encoding                 = MMAL_ENCODING_RGB24;
    format->es->video.width         = width;
    format->es->video.height        = height;
    format->es->video.crop.x        = 0;
    format->es->video.crop.y        = 0;
    format->es->video.crop.width    = width;
    format->es->video.crop.height   = height;
    format->es->video.frame_rate.num = framerate;
    format->es->video.frame_rate.den = 1;

    status = mmal_port_format_commit(video_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "Failed to set camera output format.\n");
        mmal_component_destroy(state->camera);
        return -1;
    }

    // Ensure buffer size is large enough for width*height*3 (RGB24).
    video_port->buffer_size = (video_port->buffer_size_recommended > width*height*3)
                                ? video_port->buffer_size_recommended
                                : width*height*3;
    video_port->buffer_num = (video_port->buffer_num_recommended > 3)
                                ? video_port->buffer_num_recommended
                                : 3;

    // Create a pool of buffers for this port.
    state->pool = mmal_port_pool_create(video_port, video_port->buffer_num, video_port->buffer_size);
    if (!state->pool) {
        fprintf(stderr, "Failed to create buffer pool.\n");
        mmal_component_destroy(state->camera);
        return -1;
    }

    // Create a queue to receive filled buffers via callback.
    state->callback_queue = mmal_queue_create();
    if (!state->callback_queue) {
        fprintf(stderr, "Failed to create callback queue.\n");
        mmal_port_pool_destroy(video_port, state->pool);
        mmal_component_destroy(state->camera);
        return -1;
    }

    // Set callback queue as userdata so the callback can push buffers onto it.
    video_port->userdata = (struct MMAL_PORT_USERDATA_T *)state->callback_queue;

    // Enable the video port with our callback.
    status = mmal_port_enable(video_port, camera_buffer_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "Failed to enable camera output port.\n");
        mmal_queue_destroy(state->callback_queue);
        mmal_port_pool_destroy(video_port, state->pool);
        mmal_component_destroy(state->camera);
        return -1;
    }

    // Enable the camera component itself.
    status = mmal_component_enable(state->camera);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "Failed to enable camera component.\n");
        mmal_port_disable(video_port);
        mmal_queue_destroy(state->callback_queue);
        mmal_port_pool_destroy(video_port, state->pool);
        mmal_component_destroy(state->camera);
        return -1;
    }

    // Send all the buffers from the pool to the camera output port.
    int num_buffers = state->pool->headers_num;
    for (int i = 0; i < num_buffers; i++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state->pool->queue);
        if (!buffer) {
            fprintf(stderr, "Failed to get buffer %d from pool.\n", i);
            continue;
        }
        status = mmal_port_send_buffer(video_port, buffer);
        if (status != MMAL_SUCCESS) {
            fprintf(stderr, "Failed to send buffer %d to camera output.\n", i);
            mmal_buffer_header_release(buffer);
        }
    }

    // Start capture on the video port.
    status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, 1);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "Failed to start capture.\n");
        mmal_port_disable(video_port);
        mmal_component_disable(state->camera);
        mmal_queue_destroy(state->callback_queue);
        mmal_port_pool_destroy(video_port, state->pool);
        mmal_component_destroy(state->camera);
        return -1;
    }

    printf("Camera initialized for %dx%d @ %d FPS\n", width, height, framerate);
    return 0; // success
}

/*
 * Release all resources associated with the camera.
 */
void camera_release(CameraState *state)
{
    if (!state->camera) return; // Not initialized or already destroyed

    // Stop capture (best effort).
    mmal_port_parameter_set_boolean(state->camera_output, MMAL_PARAMETER_CAPTURE, 0);

    // Disable ports and the component.
    mmal_port_disable(state->camera_output);
    mmal_component_disable(state->camera);

    // Destroy pool/queue.
    if (state->pool) {
        mmal_port_pool_destroy(state->camera_output, state->pool);
        state->pool = NULL;
    }
    if (state->callback_queue) {
        mmal_queue_destroy(state->callback_queue);
        state->callback_queue = NULL;
    }

    // Destroy the camera component.
    mmal_component_destroy(state->camera);
    state->camera = NULL;

    printf("Camera resources released.\n");
}
