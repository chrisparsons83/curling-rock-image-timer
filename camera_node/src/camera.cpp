#include "camera.h"

bool camera_init() {
    camera_config_t config = {};

    config.pin_pwdn     = CAM_PIN_PWDN;
    config.pin_reset    = CAM_PIN_RESET;
    config.pin_xclk     = CAM_PIN_XCLK;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;
    config.pin_d7       = CAM_PIN_Y9;
    config.pin_d6       = CAM_PIN_Y8;
    config.pin_d5       = CAM_PIN_Y7;
    config.pin_d4       = CAM_PIN_Y6;
    config.pin_d3       = CAM_PIN_Y5;
    config.pin_d2       = CAM_PIN_Y4;
    config.pin_d1       = CAM_PIN_Y3;
    config.pin_d0       = CAM_PIN_Y2;
    config.pin_vsync    = CAM_PIN_VSYNC;
    config.pin_href     = CAM_PIN_HREF;
    config.pin_pclk     = CAM_PIN_PCLK;

    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.ledc_timer   = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;

    // Grayscale at QQVGA — 160x120, 1 byte per pixel = 19,200 bytes/frame.
    // No JPEG encode/decode overhead. Direct pixel access.
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size   = FRAMESIZE_QQVGA;

    // Double-buffer: one frame being processed while the next is captured.
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;

    // JPEG quality unused for grayscale, but set a default.
    config.jpeg_quality = 12;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }

    // Tune sensor settings for our use case:
    // - Low-light ice rink with overhead fluorescents
    // - We want fast exposure (short shutter) to minimize motion blur
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_whitebal(sensor, 1);       // Auto white balance on
        sensor->set_awb_gain(sensor, 1);       // AWB gain on
        sensor->set_exposure_ctrl(sensor, 1);  // Auto exposure on
        sensor->set_aec2(sensor, 0);           // Disable AEC DSP (faster)
        sensor->set_gain_ctrl(sensor, 1);      // Auto gain on
        sensor->set_brightness(sensor, 0);     // Neutral brightness
        sensor->set_contrast(sensor, 1);       // Slight contrast boost
    }

    Serial.println("Camera initialized: QQVGA grayscale, double-buffered");
    return true;
}

camera_fb_t* camera_capture() {
    return esp_camera_fb_get();
}
