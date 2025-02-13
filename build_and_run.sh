#!/bin/bash

# This script builds and runs the camera demo program.

# Compile both .c files into a single executable called 'capture_test'.
gcc camera_init.c capture_test.c -o capture_test \
    -I/opt/vc/include \
    -I/opt/vc/include/interface/vcos/pthreads \
    -I/opt/vc/include/interface/mmal \
    -L/opt/vc/lib \
    -lmmal -lmmal_core -lmmal_util -lbcm_host

# If compilation succeeded, run the program.
if [ $? -eq 0 ]; then
    echo "Compilation successful. Running program..."
    ./capture_test
else
    echo "Compilation failed!"
fi
