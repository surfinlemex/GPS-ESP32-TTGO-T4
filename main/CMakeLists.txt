# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS
"main_app.c"
"ili9341/ili9341.c"
"ili9341/fonts/font.c"
"ili9341/fonts/f16f.c"
"ili9341/fonts/f24f.c"
"ili9341/fonts/f32f.c"
"ili9341/fonts/f6x8m.c"
"console/console.c"
#"wifi/wifi_manager.c"
)

set(COMPONENT_ADD_INCLUDEDIRS "." "console" "ili9341" "ili9341/fonts")
#set(COMPONENT_ADD_INCLUDEDIRS "console" "display" "display/fonts" "ili9341")


register_component()
#idf_component_register(SRCS "main_app.c" "ili9341.c" "font.c"   INCLUDE_DIRS  ".")
#idf_component_register(SRCS "main_app.c" "ili9341.c" "font.c"
#                    INCLUDE_DIRS ".")
