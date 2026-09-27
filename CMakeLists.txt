cmake_minimum_required(VERSION 3.20)

set(PICO_SDK_TAG 2.3.1)
set(PICO_BOARD pico CACHE STRING "Target board")
option(FDSWRITEUNLOCK_USB_CONSOLE "Mirror the console on USB CDC" ON)

if(DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if(NOT PICO_SDK_PATH)
    include(FetchContent)
    FetchContent_Declare(pico_sdk
        GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
        GIT_TAG ${PICO_SDK_TAG}
        GIT_SHALLOW TRUE
        GIT_SUBMODULES lib/tinyusb
        SOURCE_SUBDIR do-not-add-subdirectory
    )
    FetchContent_MakeAvailable(pico_sdk)
    set(PICO_SDK_PATH ${pico_sdk_SOURCE_DIR})
endif()

include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(fdswriteunlock C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(fdswriteunlock
    src/main.c
    src/write_gate.c
    src/edge_counter.c
    src/write_log.c
    src/console.c
)

pico_generate_pio_header(fdswriteunlock ${CMAKE_CURRENT_LIST_DIR}/src/write_stage.pio)

target_compile_options(fdswriteunlock PRIVATE -Wall -Wextra -Werror)

target_compile_definitions(fdswriteunlock PRIVATE
    PICO_STDIO_USB_STDOUT_TIMEOUT_US=2000
    PICO_STDIO_DEFAULT_CRLF=1
    PICO_DEFAULT_UART_BAUD_RATE=115200
)

target_link_libraries(fdswriteunlock PRIVATE
    pico_stdlib
    hardware_pio
    hardware_pwm
    hardware_irq
    hardware_watchdog
)

pico_set_float_implementation(fdswriteunlock compiler)
pico_set_double_implementation(fdswriteunlock compiler)
pico_set_bit_ops_implementation(fdswriteunlock compiler)
pico_set_mem_ops_implementation(fdswriteunlock compiler)
pico_set_divider_implementation(fdswriteunlock hardware)

pico_enable_stdio_uart(fdswriteunlock 1)
if(FDSWRITEUNLOCK_USB_CONSOLE)
    pico_enable_stdio_usb(fdswriteunlock 1)
endif()

pico_add_extra_outputs(fdswriteunlock)
