# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# see https://esp-idf-lib.readthedocs.io/en/latest/
set(EXTRA_COMPONENT_DIRS
        $ENV{IDF_PATH}/../esp-idf-lib/components/esp_idf_lib_helpers
        $ENV{IDF_PATH}/../esp-idf-lib/components/i2cdev
        $ENV{IDF_PATH}/../esp-idf-lib/components/aht
        $ENV{IDF_PATH}/../esp-idf-lib/components/bh1750
        )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-fdiagnostics-color=always)

project(wt-gardener)
