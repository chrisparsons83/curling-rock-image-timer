// ============================================================================
// Curling Rock Split Timer — Camera Node (Phase 1: Single Camera POC)
//
// Detects a curling rock crossing the camera's field of view at ice level.
// Outputs detection events and timing data to Serial for validation.
//
// Board: Freenove ESP32-S3-WROOM CAM (N8R8)
// ============================================================================

#include <Arduino.h>
#include <esp_timer.h>

#include "config.h"
#include "camera.h"
#include "calibration.h"
#include "detector.h"

static RockDetector detector;

// Calibration state
static int     roi_x_start;
static int     roi_x_end;
static uint8_t baseline;

// Baseline refresh tracking
static int64_t last_baseline_refresh_us = 0;
static int64_t last_detection_us        = 0;

// Frame counter for periodic FPS reporting
static uint32_t total_frames = 0;

// Detection event counter and timing
static uint32_t detection_count = 0;
static int64_t  prev_detection_us = 0;  // Previous detection crossing time

void run_calibration() {
    CalibrationResult cal = calibration_run();
    if (!cal.success) {
        Serial.println("ERROR: Calibration failed! Using defaults.");
        roi_x_start = ROI_DEFAULT_X_START;
        roi_x_end   = ROI_DEFAULT_X_END;
        baseline    = 200;  // Assume bright ice
    } else {
        roi_x_start = cal.roi_x_start;
        roi_x_end   = cal.roi_x_end;
        baseline    = cal.baseline;
    }

    detector.init(baseline, roi_x_start, roi_x_end);
    last_baseline_refresh_us = esp_timer_get_time();
    last_detection_us = 0;
}

void setup() {
    Serial.begin(SERIAL_BAUD);

    // Wait for Serial on USB CDC
    int wait_ms = 0;
    while (!Serial && wait_ms < 3000) {
        delay(100);
        wait_ms += 100;
    }

    Serial.println();
    Serial.println("===========================================");
    Serial.println("  Curling Rock Split Timer — Camera Node");
    Serial.printf("  Node ID: %d\n", NODE_ID);
    Serial.println("  Phase 1: Single Camera POC");
    Serial.println("===========================================");
    Serial.println();

    // Initialize camera
    if (!camera_init()) {
        Serial.println("FATAL: Camera init failed. Halting.");
        while (true) delay(1000);
    }

    // Discard first few frames (auto-exposure settling)
    Serial.println("Waiting for auto-exposure to settle...");
    for (int i = 0; i < 20; i++) {
        camera_fb_t* fb = camera_capture();
        if (fb) esp_camera_fb_return(fb);
        delay(50);
    }

    // Run calibration
    run_calibration();

    Serial.println();
    Serial.println("Detection active. Slide a rock through the FOV!");
    Serial.println("Serial commands: 'c' = recalibrate, 'd' = toggle debug");
    Serial.println();
}

void loop() {
    // Check for serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'c' || cmd == 'C') {
            Serial.println("\n--- Recalibrating ---");
            run_calibration();
            Serial.println("--- Recalibration complete ---\n");
        }
#ifndef DEBUG_PRINT_EVERY_FRAME
        if (cmd == 'd' || cmd == 'D') {
            Serial.println("(Debug per-frame printing is compile-time. "
                           "Uncomment DEBUG_PRINT_EVERY_FRAME in config.h)");
        }
#endif
    }

    // Capture frame
    camera_fb_t* fb = camera_capture();
    if (!fb) {
        Serial.println("WARN: Frame capture failed");
        return;
    }

    // Analyze ROI
    RoiResult roi = detector.analyze_roi(fb);

    // Feed into state machine
    DetectionEvent event = detector.update(roi);

    // Release frame buffer immediately (frees DMA for next capture)
    esp_camera_fb_return(fb);

    total_frames++;

    // Handle detection event
    if (event.detected) {
        detection_count++;

        // Format the crossing time relative to boot
        float crossing_sec = (float)event.crossing_time_us / 1000000.0f;

        Serial.println("========================================");
        Serial.printf("  ROCK DETECTED (#%u)\n", detection_count);
        Serial.printf("  Crossing time:  %.6f s (since boot)\n", crossing_sec);
        Serial.printf("  ROI brightness: %d (baseline: %d, drop: %d)\n",
                       roi.brightness, baseline,
                       (int)baseline - (int)roi.brightness);
        Serial.printf("  Confidence:     %.0f%%\n", event.confidence * 100.0f);
        Serial.printf("  Current FPS:    %.1f\n", detector.fps());

        if (detection_count >= 2 && prev_detection_us > 0) {
            // Show time since previous detection (useful for validating
            // timing with a single camera and repeated slides)
            float delta_sec = (float)(event.crossing_time_us - prev_detection_us)
                              / 1000000.0f;
            Serial.printf("  Since last:     %.3f s\n", delta_sec);
        }

        Serial.println("========================================");
        Serial.println();

        prev_detection_us = event.crossing_time_us;
        last_detection_us = event.crossing_time_us;
    }

#ifdef DEBUG_PRINT_EVERY_FRAME
    Serial.printf("ROI=%3d base=%3d state=%-8s fps=%.1f\n",
                   roi.brightness, baseline, detector.state_name(),
                   detector.fps());
#endif

    // Periodic FPS report
    if (total_frames % FPS_REPORT_INTERVAL == 0) {
        Serial.printf("[status] frames=%u detections=%u fps=%.1f state=%s "
                       "baseline=%d\n",
                       total_frames, detection_count, detector.fps(),
                       detector.state_name(), baseline);
    }

    // Periodic baseline refresh when idle
    // Only refresh if no rock has been detected recently (avoid recalibrating
    // while a rock is in view or was just seen)
    int64_t now = esp_timer_get_time();
    bool idle_long_enough = (last_detection_us == 0) ||
                            (now - last_detection_us > BASELINE_REFRESH_INTERVAL_US);
    bool refresh_due = (now - last_baseline_refresh_us) > BASELINE_REFRESH_INTERVAL_US;

    if (refresh_due && idle_long_enough &&
        detector.state() == DetectorState::IDLE) {
        baseline = calibration_refresh_baseline(roi_x_start, roi_x_end);
        detector.set_baseline(baseline);
        last_baseline_refresh_us = now;
        Serial.printf("[recal] baseline refreshed to %d\n", baseline);
    }
}
