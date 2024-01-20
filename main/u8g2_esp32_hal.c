#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "u8g2_esp32_hal.h"

static const char *TAG = "u8g2_hal";
static const unsigned int I2C_TIMEOUT_MS = 1000;

static spi_device_handle_t handle_spi;  // SPI handle.
static i2c_cmd_handle_t handle_i2c;     // I2C handle.
static u8g2_esp32_hal_t u8g2_esp32_hal; // HAL state data.

#define HOST SPI2_HOST

#undef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x)                         \
    do                                             \
    {                                              \
        esp_err_t rc = (x);                        \
        if (rc != ESP_OK)                          \
        {                                          \
            ESP_LOGE("err", "esp_err_t = %d", rc); \
            assert(0 && #x);                       \
        }                                          \
    } while (0);

/*
 * Initialze the ESP32 HAL.
 */
void u8g2_esp32_hal_init(u8g2_esp32_hal_t u8g2_esp32_hal_param)
{
    u8g2_esp32_hal = u8g2_esp32_hal_param;
} // u8g2_esp32_hal_init

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is
 * invoked to handle SPI communications.
 */
uint8_t u8g2_esp32_spi_byte_cb(u8x8_t *u8x8,
                               uint8_t msg,
                               uint8_t arg_int,
                               void *arg_ptr)
{
    ESP_LOGD(TAG, "spi_byte_cb: Received a msg: %d, arg_int: %d, arg_ptr: %p",
             msg, arg_int, arg_ptr);
    switch (msg)
    {
    case U8X8_MSG_BYTE_SET_DC:
        if (u8g2_esp32_hal.dc != U8G2_ESP32_HAL_UNDEFINED)
            gpio_set_level(u8g2_esp32_hal.dc, arg_int);
        break;

    case U8X8_MSG_BYTE_INIT:
    {
        if (u8g2_esp32_hal.bus.spi.clk == U8G2_ESP32_HAL_UNDEFINED ||
            u8g2_esp32_hal.bus.spi.mosi == U8G2_ESP32_HAL_UNDEFINED)
        {
            ESP_LOGE(TAG, "SPI bus missing GPIO config");
            break;
        }

        spi_bus_config_t bus_config;
        memset(&bus_config, 0, sizeof(spi_bus_config_t));
        bus_config.sclk_io_num = u8g2_esp32_hal.bus.spi.clk;  // CLK
        bus_config.mosi_io_num = u8g2_esp32_hal.bus.spi.mosi; // MOSI
        bus_config.miso_io_num = GPIO_NUM_NC;                 // MISO
        bus_config.quadwp_io_num = GPIO_NUM_NC;               // Not used
        bus_config.quadhd_io_num = GPIO_NUM_NC;               // Not used
        ESP_LOGI(TAG, "... Initializing bus.");
        ESP_ERROR_CHECK(spi_bus_initialize(HOST, &bus_config, SPI_DMA_CH_AUTO));

        spi_device_interface_config_t dev_config;
        memset(&dev_config, 0, sizeof(spi_device_interface_config_t));
        dev_config.address_bits = 0;
        dev_config.command_bits = 0;
        dev_config.dummy_bits = 0;
        dev_config.mode = 0;
        dev_config.duty_cycle_pos = 0;
        dev_config.cs_ena_posttrans = 0;
        dev_config.cs_ena_pretrans = 0;
        dev_config.clock_speed_hz = (10 * 1000 * 1000);
        dev_config.clock_source = SPI_CLK_SRC_DEFAULT;
        dev_config.spics_io_num = u8g2_esp32_hal.bus.spi.cs;
        dev_config.flags = 0;
        dev_config.queue_size = 200;
        dev_config.pre_cb = NULL;
        dev_config.post_cb = NULL;
        ESP_LOGI(TAG, "... Adding device bus.");
        ESP_ERROR_CHECK(spi_bus_add_device(HOST, &dev_config, &handle_spi));

        break;
    }

    case U8X8_MSG_BYTE_SEND:
    {
        static spi_transaction_t trans_desc = {
            .addr = 0,
            .cmd = 0,
            .flags = 0,
            .length = 0,
            .rx_buffer = NULL,
            .rxlength = 0,
            .tx_buffer = NULL,
            .user = 0};

        trans_desc.length = 8 * arg_int; // Number of bits NOT number of bytes.
        trans_desc.tx_buffer = arg_ptr;

        // ESP_LOGI(TAG, "... Transmitting %d bytes.", arg_int);
        ESP_ERROR_CHECK(spi_device_transmit(handle_spi, &trans_desc));
        break;
    }
    }
    return 0;
} // u8g2_esp32_spi_byte_cb

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is
 * invoked to handle I2C communications.
 */
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8,
                               uint8_t msg,
                               uint8_t arg_int,
                               void *arg_ptr)
{
    ESP_LOGD(TAG, "i2c_cb: Received a msg: %d, arg_int: %d, arg_ptr: %p", msg,
             arg_int, arg_ptr);

    switch (msg)
    {
    case U8X8_MSG_BYTE_SET_DC:
    {
        if (u8g2_esp32_hal.dc != U8G2_ESP32_HAL_UNDEFINED)
        {
            gpio_set_level(u8g2_esp32_hal.dc, arg_int);
        }
        break;
    }

    case U8X8_MSG_BYTE_INIT:
    {
        if (u8g2_esp32_hal.bus.i2c.sda == U8G2_ESP32_HAL_UNDEFINED ||
            u8g2_esp32_hal.bus.i2c.scl == U8G2_ESP32_HAL_UNDEFINED)
        {
            break;
        }

        i2c_config_t conf = {0};
        conf.mode = I2C_MODE_MASTER;
        ESP_LOGI(TAG, "sda_io_num %d", u8g2_esp32_hal.bus.i2c.sda);
        conf.sda_io_num = u8g2_esp32_hal.bus.i2c.sda;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        ESP_LOGI(TAG, "scl_io_num %d", u8g2_esp32_hal.bus.i2c.scl);
        conf.scl_io_num = u8g2_esp32_hal.bus.i2c.scl;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        ESP_LOGI(TAG, "clk_speed %d", I2C_MASTER_FREQ_HZ);
        conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
        ESP_LOGI(TAG, "i2c_param_config %d", conf.mode);
        ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
        ESP_LOGI(TAG, "i2c_driver_install %d", I2C_MASTER_NUM);
        ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                                           I2C_MASTER_RX_BUF_DISABLE,
                                           I2C_MASTER_TX_BUF_DISABLE, 0));
        break;
    }

    case U8X8_MSG_BYTE_SEND:
    {
        uint8_t *data_ptr = (uint8_t *)arg_ptr;
        ESP_LOG_BUFFER_HEXDUMP(TAG, data_ptr, arg_int, ESP_LOG_VERBOSE);

        while (arg_int > 0)
        {
            ESP_ERROR_CHECK(
                i2c_master_write_byte(handle_i2c, *data_ptr, ACK_CHECK_EN));
            data_ptr++;
            arg_int--;
        }
        break;
    }

    case U8X8_MSG_BYTE_START_TRANSFER:
    {
        uint8_t i2c_address = u8x8_GetI2CAddress(u8x8);
        handle_i2c = i2c_cmd_link_create();
        ESP_LOGD(TAG, "Start I2C transfer to %02X.", i2c_address >> 1);
        ESP_ERROR_CHECK(i2c_master_start(handle_i2c));
        ESP_ERROR_CHECK(i2c_master_write_byte(
            handle_i2c, i2c_address | I2C_MASTER_WRITE, ACK_CHECK_EN));
        break;
    }

    case U8X8_MSG_BYTE_END_TRANSFER:
    {
        ESP_LOGD(TAG, "End I2C transfer.");
        ESP_ERROR_CHECK(i2c_master_stop(handle_i2c));
        ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, handle_i2c,
                                             pdMS_TO_TICKS(I2C_TIMEOUT_MS)));
        i2c_cmd_link_delete(handle_i2c);
        break;
    }
    }
    return 0;
} // u8g2_esp32_i2c_byte_cb

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is
 * invoked to handle callbacks for GPIO and delay functions.
 */
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8,
                                     uint8_t msg,
                                     uint8_t arg_int,
                                     void *arg_ptr)
{
    ESP_LOGD(TAG,
             "gpio_and_delay_cb: Received a msg: %d, arg_int: %d, arg_ptr: %p",
             msg, arg_int, arg_ptr);

    switch (msg)
    {
        // Initialize the GPIO and DELAY HAL functions.  If the pins for DC and
        // RESET have been specified then we define those pins as GPIO outputs.
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
    {
        uint64_t bitmask = 0;
        if (u8g2_esp32_hal.dc != U8G2_ESP32_HAL_UNDEFINED)
            bitmask |= (1ull << u8g2_esp32_hal.dc);
        if (u8g2_esp32_hal.reset != U8G2_ESP32_HAL_UNDEFINED)
            bitmask |= (1ull << u8g2_esp32_hal.reset);
        if (u8g2_esp32_hal.bus.spi.cs != U8G2_ESP32_HAL_UNDEFINED)
            bitmask |= (1ull << u8g2_esp32_hal.bus.spi.cs);
        if (u8g2_esp32_hal.bus.spi.mosi != U8G2_ESP32_HAL_UNDEFINED)
            bitmask |= (1ull << u8g2_esp32_hal.bus.spi.mosi);
        if (u8g2_esp32_hal.bus.spi.clk != U8G2_ESP32_HAL_UNDEFINED)
            bitmask |= (1ull << u8g2_esp32_hal.bus.spi.clk);

        if (bitmask == 0)
            break;

        gpio_config_t io_config;
        memset(&io_config, 0, sizeof(gpio_config_t));
        io_config.pin_bit_mask = bitmask;
        io_config.mode = GPIO_MODE_OUTPUT;
        io_config.pull_up_en = GPIO_PULLUP_DISABLE;
        io_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_config.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io_config);
        break;
    }

        // Set the GPIO reset pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_RESET:
        if (u8g2_esp32_hal.reset != U8G2_ESP32_HAL_UNDEFINED)
            gpio_set_level(u8g2_esp32_hal.reset, arg_int);
        break;
        // Set the GPIO client select pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_CS:
        if (u8g2_esp32_hal.bus.spi.cs != U8G2_ESP32_HAL_UNDEFINED)
            gpio_set_level(u8g2_esp32_hal.bus.spi.cs, arg_int);
        break;
        // Set the Software I²C pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_I2C_CLOCK:
        if (u8g2_esp32_hal.bus.i2c.scl != U8G2_ESP32_HAL_UNDEFINED)
        {
            gpio_set_level(u8g2_esp32_hal.bus.i2c.scl, arg_int);
            //				printf("%c",(arg_int==1?'C':'c'));
        }
        break;
        // Set the Software I²C pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_I2C_DATA:
        if (u8g2_esp32_hal.bus.i2c.sda != U8G2_ESP32_HAL_UNDEFINED)
        {
            gpio_set_level(u8g2_esp32_hal.bus.i2c.sda, arg_int);
            //				printf("%c",(arg_int==1?'D':'d'));
        }
        break;

        // Delay for the number of milliseconds passed in through arg_int.
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(arg_int / portTICK_PERIOD_MS);
        break;
    }
    return 0;
} // u8g2_esp32_gpio_and_delay_cb