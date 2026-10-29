# secrets.h (Wi-Fi networks, the Mac's addresses, the shared secret) is the bridge's: ../esp32s3-bridge/make_secrets.py writes it
idf_component_register(SRCS "main.c" "dusb.c" "loader.c" INCLUDE_DIRS "." "../../esp32s3-bridge/main"
                       PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash lwip mbedtls esp-tls usb esp_driver_gpio
                       EMBED_FILES "payload/CALCGPT.8xp" "payload/SOLVER.8xp" "payload/clibs.8xg"
                                   "payload/arTIfiCE.8xp" "payload/INEQUVAR.8xv")
