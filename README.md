# Final Year Project

This was made as the final year project for my bachelor degree in Universitas Bunda Mulia.

This project leveraging TinyML, MQTT, and ESP32-S3 as the platform


## Urgency

In need for a private local weather station that can actually predict a data as reference.
Local climate is more or less, can be differ from mile to mile, so the idea was using IoT with TinyML for on chip inferencing, to predict the local climate.
This can also means a better "privacy", as you can use it locally, with LAN and a simple Server (You can use your old desktop/laptop, or raspberry pi... I mean, whatever that can get docker running), or just display it with something like TFT display (Expensive).


## A Mean To Replicate

What do you need:
- ESP32S3 8r8n (In this case, i use XIAO SEEED ESP32S3, YDESP32S3 works fine tho, but you need to adjust the WiFi transmission power)
- BME280 i2c (For temperature, humidity, and pressure)


## Data
My model was adjusted to the North Jakarta (Sunda Kelapa) Climate.
The data is ranging from February 2020 to August 2025.
All the data was collected via Open-Meteo Historical Data.
[open-meteo](https://open-meteo.com/)


## Step to replicate
1. Adjust your SDA and SCL on the i2c_initialization() function accordingly
2. Place your model binary into the model_data.cc
3. ``idf.py menuconfig``  -> ``Serial Flasher Config`` -> ``Flash Size(8MB)``
4. ``idf.py menuconfig`` -> ``Component Config`` -> ``ESP-PSRAM`` -> ``[*] Support for external, SPI-connected RAM`` -> ``SPI RAM Config`` -> ``Mode of SPI RAM Chip in use (Octal Mode PSRAM)`` -> ``Set RAM clock speed (80MHz Clock Speed)``
5. ``idf.py Partition Table (Custom Partition Table CSV)``
6. ``idf.py build``
7. ``idf.py flash``
8. (Optional) ``idf.py monitor``
