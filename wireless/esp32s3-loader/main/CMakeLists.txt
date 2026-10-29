# The box = the loader (dusb.c, loader.c, payload.c) + the bridge from ../esp32s3-bridge (bridge.c, protocol.c, calclink.c) over
# either USB role: link_cdc.c (host) or the bridge's link_jtag.c (device), chosen by role.c (box_pins.h: BOX_HW).
# secrets.h (Wi-Fi networks, servers, secrets) is the bridge's: ../esp32s3-bridge/make_secrets.py writes it.
# payload/: filled by ../fill_payload.sh (built programs, TI's Inequalz app, arTIfiCE v2.1 and v3); not in git
set(BRIDGE ../../esp32s3-bridge/main)
idf_component_register(SRCS "main.c" "dusb.c" "loader.c" "payload.c" "link_cdc.c" "role.c"
                            "${BRIDGE}/bridge.c" "${BRIDGE}/protocol.c" "${BRIDGE}/calclink.c" "${BRIDGE}/link_jtag.c"
                       INCLUDE_DIRS "." "${BRIDGE}"
                       PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash lwip mbedtls esp-tls usb esp_driver_gpio esp_driver_usb_serial_jtag
                       EMBED_FILES "payload/clibs.8xg" "payload/CALC.8xp" "payload/CALCGPT.8xp" "payload/SOLVER.8xp"
                                   "payload/arTIfiCE2.8xp" "payload/arTIfiCE.8xp" "payload/INEQUVAR.8xv" "payload/Inequalz.8ek")
