# secrets.h (Wi-Fi networks, the Mac's addresses) is the bridge's: ../esp32s3-bridge/make_secrets.py writes it
idf_component_register(SRCS "main.c" INCLUDE_DIRS "." "../../esp32s3-bridge/main"
                       PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash lwip)
