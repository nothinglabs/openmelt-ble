#include "accel.h"

#include "h3lis331dl_reg.h"

// https://github.com/STMicroelectronics/h3lis331dl-pid/tree/d2404b332f7ba6f517b6b959e09014d429cac9ae?tab=readme-ov-file
// https://github.com/STMicroelectronics/STMems_Standard_C_drivers/tree/master/h3lis331dl_STdC/examples

#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>

#include <zephyr/drivers/i2c.h>

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

struct k_mutex accel_mutex;

#define I2C_ADDRESS 0x19

/* Private macro -------------------------------------------------------------*/
#define BOOT_TIME 5 // ms
/* Private variables ---------------------------------------------------------*/
static int16_t data_raw_acceleration[3];
static float acceleration_mg[3];
static uint8_t whoamI;
static uint8_t tx_buffer[1000];

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay)
#define I2C_DEV_NODE DT_NODELABEL(i2c1)
#else
#error "Please set the correct I2C device"
#endif

uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

const struct device *i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);

stmdev_ctx_t dev_ctx;

static float sampled_accel_value_g = 0.0f;

void update_accel_value()
{
  static float accel = 0;

  /* Read acceleration data */
  memset(data_raw_acceleration, 0x00, 3 * sizeof(int16_t));
  h3lis331dl_acceleration_raw_get(&dev_ctx, data_raw_acceleration);

  accel = h3lis331dl_from_fs200_to_mg(data_raw_acceleration[0]);
  accel = accel / 1000.0f;

  k_mutex_lock(&accel_mutex, K_FOREVER);
  if (sampled_accel_value_g == 0) sampled_accel_value_g = accel;
  
  //slight moving average smoothing on accel
  sampled_accel_value_g = sampled_accel_value_g * 0.5f + accel * .5f;
  k_mutex_unlock(&accel_mutex);
}

static float current_accel = 0.0f;

float get_accel_g()
{
  k_mutex_lock(&accel_mutex, K_FOREVER);
  current_accel = sampled_accel_value_g;
  k_mutex_unlock(&accel_mutex);
  return current_accel;
}

int32_t platform_write(void *handle, uint8_t Reg, const uint8_t *Bufp, uint16_t len)
{

  uint8_t buf[len + 1]; // Buffer to hold the register address and the data to write

  Reg |= 0x80;

  // First byte is the register address
  buf[0] = Reg;

  // Copy the data to write into the buffer, starting from the second byte
  memcpy(&buf[1], Bufp, len);

  // Perform the I2C write operation
  int ret = i2c_write(i2c_dev, buf, len + 1, I2C_ADDRESS);
  if (ret < 0)
  {
    printk("Failed to write data to register\n");
    return ret;
  }

  return 0; // Success
}

int32_t platform_read(void *handle, uint8_t Reg, uint8_t *Bufp, uint16_t len)
{

  Reg |= 0x80;

  // Write the register address
  int ret = i2c_write(i2c_dev, &Reg, 1, I2C_ADDRESS);
  if (ret < 0)
  {
    printk("Failed to write register address\n");
    return ret;
  }

  // Read from the register
  ret = i2c_read(i2c_dev, Bufp, len, I2C_ADDRESS);
  if (ret < 0)
  {
    printk("Failed to read data\n");
    return ret;
  }

  return 0; // Success
}

static void platform_delay(uint32_t ms)
{
  k_sleep(K_MSEC(ms));
}

void init_accel()
{
  k_mutex_init(&accel_mutex);

  i2c_configure(i2c_dev, i2c_cfg);

  /* Initialize mems driver interface */
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.mdelay = platform_delay;
  dev_ctx.handle = NULL;

  /* Wait sensor boot time */
  platform_delay(BOOT_TIME);

  /* Check device ID */
  h3lis331dl_device_id_get(&dev_ctx, &whoamI);

  if (whoamI != H3LIS331DL_ID)
  {
    while (1)
    {
      platform_delay(BOOT_TIME);
    }
  }

  /* Enable Block Data Update */
  h3lis331dl_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
  /* Set full scale */
  h3lis331dl_full_scale_set(&dev_ctx, H3LIS331DL_200g);
  /* Set Output Data Rate */
  h3lis331dl_data_rate_set(&dev_ctx, H3LIS331DL_ODR_5Hz);
}

void accel_data_polling(void)
{

  /* Read samples in polling mode (no int) */
  while (1)
  {

    /* Read output only if new value is available */
    h3lis331dl_reg_t reg;
    h3lis331dl_status_reg_get(&dev_ctx, &reg.status_reg);

    if (reg.status_reg.zyxda)
    {
      /* Read acceleration data */
      memset(data_raw_acceleration, 0x00, 3 * sizeof(int16_t));
      h3lis331dl_acceleration_raw_get(&dev_ctx, data_raw_acceleration);

      acceleration_mg[0] =
          h3lis331dl_from_fs200_to_mg(data_raw_acceleration[0]);
      acceleration_mg[1] =
          h3lis331dl_from_fs200_to_mg(data_raw_acceleration[1]);
      acceleration_mg[2] =
          h3lis331dl_from_fs200_to_mg(data_raw_acceleration[2]);

      sprintf((char *)tx_buffer,
              "Acceleration [mg]:%4.2f\t%4.2f\t%4.2f\r\n",
              acceleration_mg[0], acceleration_mg[1], acceleration_mg[2]);
    }
  }
}