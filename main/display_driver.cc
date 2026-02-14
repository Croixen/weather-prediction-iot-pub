#include "display_driver.h"
#include "bme280_driver.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include <stdio.h>
#include <string.h>

#define TAG "[DISPLAY]"

#define OLED_I2C_ADDRESS    0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

#define SSD1306_CMD_DISPLAY_OFF         0xAE
#define SSD1306_CMD_DISPLAY_ON          0xAF
#define SSD1306_CMD_SET_CONTRAST        0x81
#define SSD1306_CMD_SET_COLUMN_ADDR     0x21
#define SSD1306_CMD_SET_PAGE_ADDR       0x22

static bool display_powered_on = false;
static bool display_initialized = false;
static i2c_bus_device_handle_t ssd1306_handle = NULL;

static esp_err_t ssd1306_write_command(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};  
    return i2c_bus_write_bytes(ssd1306_handle, 0x00, 1, &cmd);
}

static void ssd1306_clear_display(void) {
    ssd1306_write_command(SSD1306_CMD_SET_COLUMN_ADDR);
    ssd1306_write_command(0);
    ssd1306_write_command(127);
    
    ssd1306_write_command(SSD1306_CMD_SET_PAGE_ADDR);
    ssd1306_write_command(0);
    ssd1306_write_command(7);
    
    uint8_t clear_data[128];
    for (int i = 0; i < 128; i++) {
        clear_data[i] = 0x00;
    }
    
    for (int page = 0; page < 8; page++) {
        i2c_bus_write_bytes(ssd1306_handle, 0x40, 128, clear_data);
    }
}

static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0 (48)
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A (65)
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a (97)
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
};

/**
 * Draw a single character at position
 */
static void ssd1306_draw_char(uint8_t x, uint8_t page, char c) {
    if (c < 32 || c > 122) {
        c = ' ';  
    }
    
    uint8_t char_index = c - 32;
    uint8_t char_data[6];
    
    
    for (int i = 0; i < 5; i++) {
        char_data[i] = font_5x7[char_index][i];
    }
    char_data[5] = 0x00;  
    
    
    ssd1306_write_command(SSD1306_CMD_SET_COLUMN_ADDR);
    ssd1306_write_command(x);
    ssd1306_write_command(x + 5);
    
    
    ssd1306_write_command(SSD1306_CMD_SET_PAGE_ADDR);
    ssd1306_write_command(page);
    ssd1306_write_command(page);
    
    
    i2c_bus_write_bytes(ssd1306_handle, 0x40, 6, char_data);
}


static void ssd1306_draw_text(uint8_t x, uint8_t page, const char* text) {
    uint8_t pos_x = x;
    
    for (int i = 0; text[i] != '\0'; i++) {
        if (pos_x > 122) break;  
        
        ssd1306_draw_char(pos_x, page, text[i]);
        pos_x += 6;  
    }
    
    ESP_LOGI(TAG, "Display [%d,%d]: %s", x, page, text);
}


esp_err_t display_init(void) {
    i2c_bus_handle_t bus = get_i2c_bus_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initializing SSD1306 display...");
    
    
    ssd1306_handle = i2c_bus_device_create(bus, OLED_I2C_ADDRESS, 0);
    if (ssd1306_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create SSD1306 device handle");
        return ESP_FAIL;
    }
    
    // Init sequence for SSD1306
    ssd1306_write_command(SSD1306_CMD_DISPLAY_OFF);
    ssd1306_write_command(0xD5);
    ssd1306_write_command(0x80);
    ssd1306_write_command(0xA8); 
    ssd1306_write_command(0x3F);
    ssd1306_write_command(0xD3); 
    ssd1306_write_command(0x00);
    ssd1306_write_command(0x40); 
    ssd1306_write_command(0x8D); 
    ssd1306_write_command(0x14);
    ssd1306_write_command(0x20); 
    ssd1306_write_command(0x00);
    ssd1306_write_command(0xA1); 
    ssd1306_write_command(0xC8); 
    ssd1306_write_command(0xDA); 
    ssd1306_write_command(0x12);
    ssd1306_write_command(SSD1306_CMD_SET_CONTRAST);
    ssd1306_write_command(0xCF);
    ssd1306_write_command(0xD9); 
    ssd1306_write_command(0xF1);
    ssd1306_write_command(0xDB); 
    ssd1306_write_command(0x40);
    ssd1306_write_command(0xA4); 
    ssd1306_write_command(0xA6);
    
    ssd1306_clear_display();
    
    display_off();
    
    display_initialized = true;
    ESP_LOGI(TAG, "Display initialized successfully (OFF by default)");
    return ESP_OK;
}

void display_clear(void) {
    if (display_initialized) {
        ssd1306_clear_display();
    }
}

void display_show_page(uint8_t page_index, const t_infered* prediction_data, const t_bme280_s_val* sensor_data) {
    if (!display_initialized) {
        return;
    }

    if (page_index > DISPLAY_PAGE_MAX) {
        page_index = DISPLAY_PAGE_MAX;
    }

    char buffer[64];
    
    ssd1306_clear_display();
    
    if (page_index == 0) {
        ssd1306_draw_text(43, 0, "CURRENT");
        ssd1306_draw_text(0, 1, "---------------------");
        
        if (sensor_data != NULL) {
            snprintf(buffer, sizeof(buffer), " Temp: %.1f C", sensor_data->temperature);
            ssd1306_draw_text(0, 2, buffer);
            
            snprintf(buffer, sizeof(buffer), " Hum : %.1f %%", sensor_data->humidity);
            ssd1306_draw_text(0, 4, buffer);
            
            snprintf(buffer, sizeof(buffer), " Pres: %.0f hPa", sensor_data->pressure);
            ssd1306_draw_text(0, 6, buffer);
        } else {
            ssd1306_draw_text(10, 3, "No Data");
        }
    } 

    else {
        int pred_idx = page_index - 1;
        
        snprintf(buffer, sizeof(buffer), "+%d HOUR PREDICTION", page_index);

        snprintf(buffer, sizeof(buffer), "+%d HOUR", page_index);
        int len = strlen(buffer);
        int x = (128 - (len * 6)) / 2;
        ssd1306_draw_text(x, 0, buffer);
        
        ssd1306_draw_text(0, 1, "---------------------");
        
        if (prediction_data != NULL) {
            snprintf(buffer, sizeof(buffer), " Temp: %.1f C", prediction_data->predicted_data[pred_idx].temperature);
            ssd1306_draw_text(0, 2, buffer);
            
            snprintf(buffer, sizeof(buffer), " Hum : %.1f %%", prediction_data->predicted_data[pred_idx].humidity);
            ssd1306_draw_text(0, 4, buffer);
            
            snprintf(buffer, sizeof(buffer), " Pres: %.0f hPa", prediction_data->predicted_data[pred_idx].pressure);
            ssd1306_draw_text(0, 6, buffer);
        } else {
             ssd1306_draw_text(10, 3, "No Prediction");
        }
    }
}

void display_show_network_status(bool connected, const char* ip_address, int buffer_size) {
    if (!display_initialized) {
        return;
    }

    char buffer[64];
    
    ssd1306_clear_display();

    ssd1306_draw_text(22, 0, "NETWORK STATUS");
    ssd1306_draw_text(0, 1, "---------------------");
    
    // Connection status
    if (connected) {
        ssd1306_draw_text(0, 3, "Status: Connected");
        if (ip_address != NULL) {
            snprintf(buffer, sizeof(buffer), "IP: %s", ip_address);
            ssd1306_draw_text(0, 4, buffer);
        }
    } else {
        ssd1306_draw_text(0, 3, "Status: Disconnected");
    }

    // Buffer size
    snprintf(buffer, sizeof(buffer), "Buffer: %d / 24", buffer_size);
    ssd1306_draw_text(0, 6, buffer);
}

void display_show_sleep_warning(void) {
    if (!display_initialized) {
        return;
    }

    ssd1306_clear_display();
    
    ssd1306_draw_text(10, 2, "SLEEP MODE");
    ssd1306_draw_text(0, 4, "Release to sleep");
}

void display_on(void) {
    if (!display_initialized) {
        return;
    }
    
    if (!display_powered_on) {
        ssd1306_write_command(SSD1306_CMD_DISPLAY_ON);
        display_powered_on = true;
        ESP_LOGI(TAG, "Display turned ON");
    }
}

void display_off(void) {
    if (!display_initialized) {
        return;
    }
    
    ssd1306_clear_display();
    ssd1306_write_command(SSD1306_CMD_DISPLAY_OFF);
    display_powered_on = false;
    ESP_LOGI(TAG, "Display turned OFF");
}

bool display_is_on(void) {
    return display_powered_on;
}
