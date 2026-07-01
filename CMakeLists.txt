set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(SIZE arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(i2c_fg_mock C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

add_compile_options(-mcpu=cortex-m3 -mthumb -mthumb-interwork)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)
add_compile_options($<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")
    add_compile_options(-Ofast)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "Maximum optimization for speed, debug info included")
    add_compile_options(-Ofast -g)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)
else ()
    message(STATUS "Minimal optimization, debug info included")
    add_compile_options(-Og -g)
    add_definitions(-DDEBUG)
endif ()

set(CUBE_FW "C:/Users/Thunder/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.4")
set(HAL_INC "${CUBE_FW}/Drivers/STM32F1xx_HAL_Driver/Inc")
set(HAL_SRC "${CUBE_FW}/Drivers/STM32F1xx_HAL_Driver/Src")
set(CMSIS_INC "${CUBE_FW}/Drivers/CMSIS")
set(USB_LIB "${CUBE_FW}/Middlewares/ST/STM32_USB_Device_Library")

include_directories(
    Core/Inc
    USB
    USB/App
    USB/Target
    ${HAL_INC}
    ${HAL_INC}/Legacy
    ${CMSIS_INC}/Device/ST/STM32F1xx/Include
    ${CMSIS_INC}/Include
    ${USB_LIB}/Core/Inc
    ${USB_LIB}/Class/CDC/Inc
)

add_definitions(-DUSE_HAL_DRIVER -DSTM32F103xB)

file(GLOB_RECURSE APP_SOURCES "Core/*.*" "USB/*.*")

set(HAL_SOURCES
    ${HAL_SRC}/stm32f1xx_hal.c
    ${HAL_SRC}/stm32f1xx_hal_cortex.c
    ${HAL_SRC}/stm32f1xx_hal_dma.c
    ${HAL_SRC}/stm32f1xx_hal_exti.c
    ${HAL_SRC}/stm32f1xx_hal_flash.c
    ${HAL_SRC}/stm32f1xx_hal_flash_ex.c
    ${HAL_SRC}/stm32f1xx_hal_gpio.c
    ${HAL_SRC}/stm32f1xx_hal_gpio_ex.c
    ${HAL_SRC}/stm32f1xx_hal_i2c.c
    ${HAL_SRC}/stm32f1xx_hal_pcd.c
    ${HAL_SRC}/stm32f1xx_hal_pcd_ex.c
    ${HAL_SRC}/stm32f1xx_hal_pwr.c
    ${HAL_SRC}/stm32f1xx_hal_rcc.c
    ${HAL_SRC}/stm32f1xx_hal_rcc_ex.c
    ${HAL_SRC}/stm32f1xx_ll_usb.c
)

set(USB_SOURCES
    ${USB_LIB}/Core/Src/usbd_core.c
    ${USB_LIB}/Core/Src/usbd_ctlreq.c
    ${USB_LIB}/Core/Src/usbd_ioreq.c
    ${USB_LIB}/Class/CDC/Src/usbd_cdc.c
)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F103C8TX_FLASH.ld)

add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=cortex-m3 -mthumb -mthumb-interwork)
add_link_options(-T ${LINKER_SCRIPT})

add_executable(${PROJECT_NAME}.elf
    ${APP_SOURCES}
    ${HAL_SOURCES}
    ${USB_SOURCES}
    ${LINKER_SCRIPT}
)

set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
    COMMENT "Building ${HEX_FILE}\nBuilding ${BIN_FILE}")
