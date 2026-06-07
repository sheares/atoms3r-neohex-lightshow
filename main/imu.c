#include "imu.h"
#include "bmi270_cfg.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

/* Confirmed by I2C bus scan on AtomS3R */
#define IMU_SCL_GPIO    0
#define IMU_SDA_GPIO    45
#define IMU_I2C_PORT    I2C_NUM_0
#define BMI270_ADDR     0x68

/* BMI270 register addresses */
#define REG_CHIP_ID     0x00
#define REG_ACC_X_LSB   0x0C
#define REG_INT_STATUS  0x21
#define REG_INIT_CTRL   0x59
#define REG_INIT_DATA   0x5B
#define REG_ACC_CONF    0x40
#define REG_ACC_RANGE   0x41
#define REG_PWR_CONF    0x7C
#define REG_PWR_CTRL    0x7D

#define BMI270_CHIP_ID  0x24
#define ACC_SENS        16384.0f   /* ±2 g: 16384 LSB/g */
#define EMA_ALPHA       0.15f
#define TILT_THRESHOLD  0.15f      /* ~9° dead-zone when flat */

static const char *TAG = "imu";
static bool imu_ready = false;

/* --- low-level I2C helpers using legacy driver ---------------------------- */

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMI270_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(IMU_I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    /* Write phase: set register pointer */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMI270_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    /* Repeated START then read */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMI270_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(IMU_I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t config_upload(void)
{
    /* Single I2C burst: register address + 8192 config bytes */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BMI270_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, REG_INIT_DATA, true);
    i2c_master_write(cmd, bmi270_config_file, 8192, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(IMU_I2C_PORT, cmd, pdMS_TO_TICKS(5000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* --- public API ---------------------------------------------------------- */

int imu_init(void)
{
    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = IMU_SDA_GPIO,
        .scl_io_num       = IMU_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    if (i2c_param_config(IMU_I2C_PORT, &cfg) != ESP_OK ||
        i2c_driver_install(IMU_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0) != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed");
        return -1;
    }

    /* Allow BMI270 to finish power-on reset */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Verify chip ID with retries */
    uint8_t chip_id = 0;
    esp_err_t id_err = ESP_FAIL;
    for (int retry = 0; retry < 10; retry++) {
        chip_id = 0;
        id_err = reg_read(REG_CHIP_ID, &chip_id, 1);
        ESP_LOGI(TAG, "chip_id[%d]: err=0x%x val=0x%02x", retry, id_err, chip_id);
        if (id_err == ESP_OK && chip_id == BMI270_CHIP_ID) break;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (id_err != ESP_OK || chip_id != BMI270_CHIP_ID) {
        ESP_LOGE(TAG, "BMI270 not found (chip_id=0x%02x)", chip_id);
        return -1;
    }
    ESP_LOGI(TAG, "BMI270 found");

    /* Initialization sequence per BMI270 datasheet */
    reg_write(REG_PWR_CONF, 0x00);
    vTaskDelay(pdMS_TO_TICKS(1));
    reg_write(REG_INIT_CTRL, 0x00);
    if (config_upload() != ESP_OK) {
        ESP_LOGE(TAG, "config upload failed");
        return -1;
    }
    reg_write(REG_INIT_CTRL, 0x01);

    /* Wait for init_ok (bit 0 of reg 0x21), max 150 ms */
    for (int i = 0; i < 15; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t status = 0;
        reg_read(REG_INT_STATUS, &status, 1);
        ESP_LOGI(TAG, "init status[%d]=0x%02x", i, status);
        if (status & 0x01) break;
        if (i == 14) {
            ESP_LOGE(TAG, "BMI270 init timeout");
            return -1;
        }
    }

    reg_write(REG_ACC_CONF,  0xA8);  /* normal perf, 100 Hz, OSR4 */
    reg_write(REG_ACC_RANGE, 0x00);  /* ±2 g */
    reg_write(REG_PWR_CTRL,  0x04);  /* enable accel */
    vTaskDelay(pdMS_TO_TICKS(2));

    ESP_LOGI(TAG, "BMI270 ready");
    imu_ready = true;
    return 0;
}

int imu_read_accel(float *ax, float *ay, float *az)
{
    if (!imu_ready) return -1;
    uint8_t raw[6];
    if (reg_read(REG_ACC_X_LSB, raw, 6) != ESP_OK) return -1;

    int16_t rx = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t ry = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t rz = (int16_t)((raw[5] << 8) | raw[4]);

    static float fx = 0.0f, fy = 0.0f, fz = 1.0f;
    fx = EMA_ALPHA * (rx / ACC_SENS) + (1.0f - EMA_ALPHA) * fx;
    fy = EMA_ALPHA * (ry / ACC_SENS) + (1.0f - EMA_ALPHA) * fy;
    fz = EMA_ALPHA * (rz / ACC_SENS) + (1.0f - EMA_ALPHA) * fz;

    *ax = fx;
    *ay = fy;
    *az = fz;
    return 0;
}

uint16_t imu_hue_offset(float ax, float ay, float az)
{
    (void)az;
    float tilt = sqrtf(ax * ax + ay * ay);
    if (tilt < TILT_THRESHOLD) return 0;

    float angle = atan2f(ay, ax) * (180.0f / (float)M_PI) + 180.0f;
    return (uint16_t)angle % 360;
}
