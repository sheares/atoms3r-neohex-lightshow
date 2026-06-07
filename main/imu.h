#pragma once
#include <stdint.h>

/* Initialize I2C bus and BMI270 IMU.
   Returns 0 on success, -1 on failure (chip not found / init timeout). */
int imu_init(void);

/* Read accelerometer values in g, with internal exponential smoothing.
   Returns 0 on success, -1 on I2C error. */
int imu_read_accel(float *ax, float *ay, float *az);

/* Convert filtered accel vector to a hue offset (0-359).
   Returns 0 when the device is nearly flat (< ~9° tilt). */
uint16_t imu_hue_offset(float ax, float ay, float az);
