#include "accel.h"

#include "lis331dlh_reg.h"

// https://github.com/STMicroelectronics/lis331dlh-pid/tree/aecbf07a5496d1c4e7c9815b54bc5f9aeadb22bf

#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>

#include <zephyr/drivers/i2c.h>

#define I2C_ADDRESS 0x19

/* Private macro -------------------------------------------------------------*/
#define    BOOT_TIME          5 //ms
/* Private variables ---------------------------------------------------------*/
static int16_t data_raw_acceleration[3];
static float acceleration_mg[3];
static uint8_t whoamI;
static uint8_t tx_buffer[1000];


#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay)
#define I2C_DEV_NODE	DT_NODELABEL(i2c1)
#else
#error "Please set the correct I2C device"
#endif

uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

const struct device *i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);

int32_t platform_write(void *handle, uint8_t Reg, const uint8_t *Bufp, uint16_t len){

    uint8_t buf[len + 1]; // Buffer to hold the register address and the data to write

    Reg |= 0x80;

    // First byte is the register address
    buf[0] = Reg;

    // Copy the data to write into the buffer, starting from the second byte
    memcpy(&buf[1], Bufp, len);

    // Perform the I2C write operation
    int ret = i2c_write(i2c_dev, buf, len + 1, I2C_ADDRESS);
    if (ret < 0) {
        printk("Failed to write data to register\n");
        return ret;
    }

    return 0; // Success

}

               
int32_t platform_read(void *handle, uint8_t Reg, uint8_t *Bufp, uint16_t len) {

    Reg |= 0x80;

    // Write the register address
    int ret = i2c_write(i2c_dev, &Reg, 1, I2C_ADDRESS);
    if (ret < 0) {
        printk("Failed to write register address\n");
        return ret;
    }

    // Read from the register
    ret = i2c_read(i2c_dev, Bufp, len, I2C_ADDRESS);
    if (ret < 0) {
        printk("Failed to read data\n");
        return ret;
    }

    return 0; // Success
}

static void platform_delay(uint32_t ms)
{
    k_sleep(K_MSEC(ms));
}

static void platform_init(void)
{
}


void lis331dlh_read_data_polling(void)
{

  i2c_configure(i2c_dev, i2c_cfg);
  
  /* Initialize mems driver interface */
  stmdev_ctx_t dev_ctx;
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.mdelay = platform_delay;
  dev_ctx.handle = NULL;
  
  /* Wait sensor boot time */
  platform_delay(BOOT_TIME);
  
  /* Check device ID */
  lis331dlh_device_id_get(&dev_ctx, &whoamI);

  if (whoamI != LIS331DLH_ID) {
    while (1) {
      platform_delay(BOOT_TIME);
    }
  }

  platform_delay(BOOT_TIME);

  /* Enable Block Data Update */
  lis331dlh_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
  /* Set full scale */
  lis331dlh_full_scale_set(&dev_ctx, LIS331DLH_2g);
  /* Configure filtering chain */
  /* Accelerometer - High Pass / Slope path */
  //lis331dlh_hp_path_set(&dev_ctx, LIS331DLH_HP_DISABLE);
  //lis331dlh_hp_path_set(&dev_ctx, LIS331DLH_HP_ON_OUT);
  //lis331dlh_hp_reset_get(&dev_ctx);
  /* Set Output Data Rate */
  lis331dlh_data_rate_set(&dev_ctx, LIS331DLH_ODR_5Hz);

  /* Read samples in polling mode (no int) */
  while (1) {

    /* Read output only if new value is available */
    lis331dlh_reg_t reg;
    lis331dlh_status_reg_get(&dev_ctx, &reg.status_reg);

    if (reg.status_reg.zyxda) 
    {
      /* Read acceleration data */
      memset(data_raw_acceleration, 0x00, 3 * sizeof(int16_t));
      lis331dlh_acceleration_raw_get(&dev_ctx, data_raw_acceleration);

      acceleration_mg[0] =
        lis331dlh_from_fs8_to_mg(data_raw_acceleration[0]);
      acceleration_mg[1] =
        lis331dlh_from_fs8_to_mg(data_raw_acceleration[1]);
      acceleration_mg[2] =
        lis331dlh_from_fs8_to_mg(data_raw_acceleration[2]);

    

      sprintf((char *)tx_buffer,
              "Acceleration [mg]:%4.2f\t%4.2f\t%4.2f\r\n",
              acceleration_mg[0], acceleration_mg[1], acceleration_mg[2]);
    }
  }
}