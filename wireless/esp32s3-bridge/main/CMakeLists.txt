idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash lwip mbedtls esp-tls esp_driver_usb_serial_jtag)
