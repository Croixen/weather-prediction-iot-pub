#pragma once

#ifndef WIFI_HELPER_H

    #define WIFI_HELPER_H

    #include "esp_err.h"

    void wifi_init(void);

    esp_err_t wifi_connect(char* wifi_ssid, char* wifi_password);

    void wifi_disconnect(void);

    void deinit_wifi(void);

    // Get current IP address as string
    const char* wifi_get_ip_address(void);

#endif