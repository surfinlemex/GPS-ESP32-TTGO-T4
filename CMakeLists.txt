# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(MDF_VER command bash -c "cd $ENV{MDF_PATH} && git describe --always --tags --dirty")
#CPPFLAGS := -D MDF_VER=\"$(MDF_VER)\"

set(EXTRA_COMPONENT_DIRS $ENV{MDF_PATH}/components $ENV{MDF_PATH}/components/third_party)
#set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS} )
message(STATUS "EXTRA_COMPONENT_DIRS: ${EXTRA_COMPONENT_DIRS}")

set(CMAKE_SYSROOT $ENV{IDF_PATH}/../xtensa-esp32-elf/xtensa-esp32-elf)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gps-ttgo-t4)
