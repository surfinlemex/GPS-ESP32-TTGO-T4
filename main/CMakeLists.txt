# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES esp_driver_spi esp_driver_gpio esp_psram esp_wifi)
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS
"main_app.c"
"display.c"
"console/console.c"
)

set(COMPONENT_ADD_INCLUDEDIRS "." "console")
#set(COMPONENT_ADD_INCLUDEDIRS "console" "display" "display/fonts" "ili9341")


register_component()
#idf_component_register(SRCS "main_app.c" "ili9341.c" "font.c"   INCLUDE_DIRS  ".")
#idf_component_register(SRCS "main_app.c" "ili9341.c" "font.c"
#                    INCLUDE_DIRS ".")
