#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include "camera_init.h"

/*
 * A simple function to invert colors in the buffer. We declare it here
 * or you could keep it inside camera_init if you prefer.
 */
static void invert_colors(uint8_t *data, int width, int height)
{
    for (int i = 0; i < width * height * 3; i++) {
        data[i] = 255 - data[i];
    }
}

int main()
{
    CameraState state;
    int width = 640, height = 480, fps = 30;

    // Initialize the camera
    if (camera_init(&state, width, height, fps) != 0) {
        fprintf(stderr, "Camera initialization failed.\n");
        return 1;
    }

    // Capture frames for 2 seconds and count them.
    struct timeval start, now;
    gettimeofday(&start, NULL);
    double elapsed = 0.0;
    int frame_count = 0;

    while (elapsed < 2.0) {
        // Non-blocking queue check
        MMAL_BUFFER_HEADER_T *filled_buffer = mmal_queue_get(state.callback_queue);
        if (filled_buffer) {
            if (filled_buffer->length > 0) {
                // Process the image (invert colors).
                mmal_buffer_header_mem_lock(filled_buffer);
                invert_colors(filled_buffer->data, state.width, state.height);
                mmal_buffer_header_mem_unlock(filled_buffer);
                frame_count++;
            }

            // Re-send the buffer so we can get more frames.
            if (mmal_port_send_buffer(state.camera_output, filled_buffer) != MMAL_SUCCESS) {
                fprintf(stderr, "Failed to re-send buffer.\n");
                mmal_buffer_header_release(filled_buffer);
            }
        } else {
            // No frame available this iteration; sleep briefly.
            usleep(10000); // 10 ms
        }

        // Update elapsed time
        gettimeofday(&now, NULL);
        elapsed = (now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec) / 1000000.0;
    }

    // Print stats
    printf("Captured %d frames in %.2f seconds. (%.2f fps)\n",
           frame_count, elapsed, (frame_count / elapsed));

    // Cleanup
    camera_release(&state);

    return 0;
}
