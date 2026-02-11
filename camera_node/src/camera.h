#pragma once

#include <esp_camera.h>
#include "config.h"

// Initialize the OV2640 camera in grayscale QQVGA mode.
// Returns true on success.
bool camera_init();

// Capture a single frame. Caller must call esp_camera_fb_return(fb) when done.
// Returns nullptr on failure.
camera_fb_t* camera_capture();
