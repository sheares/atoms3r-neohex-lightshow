# atoms3r-neohex-lightshow

A motion-reactive light show on a NeoHEX 37-LED WS2812 ring driven by an M5Stack ATOM S3R. Built on ESP-IDF v6 and the `led_strip` v3 component as a small physical-computing exercise in addressable-LED choreography.

## What it is

The NeoHEX is a hexagonal arrangement of 37 WS2812 LEDs (rows of 4-5-6-7-6-5-4, centre at index 18). The ATOM S3R reads its onboard BMI270 IMU and translates tilt and acceleration into ring patterns: orbiting trails, breathing rings, motion-driven hue sweeps. The point is treating physical computing as a creative medium rather than a debug surface.

## Hardware

- M5Stack ATOM S3R (ESP32-S3, BMI270 IMU, GPIO 2 Grove data line driving the NeoHEX)
- M5Stack NeoHEX 1x37 WS2812B ring

## Stack

- ESP-IDF v6
- `espressif/led_strip` v3 component (`LED_STRIP_COLOR_COMPONENT_FMT_GRB`), RMT backend
- BMI270 driver for IMU readouts feeding the lightshow generator in `main/lightshow.c`

## Status

Personal project, working firmware. Useful starting point for anyone wanting an ESP-IDF v6 reference for addressable-LED work paired with onboard IMU input.
