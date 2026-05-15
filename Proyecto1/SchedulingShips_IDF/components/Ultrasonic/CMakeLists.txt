idf_component_register(
        SRCS ultrasonic.c
        INCLUDE_DIRS .
        REQUIRES driver freertos esp_rom esp_idf_lib_helpers esp_timer esp_driver_gpio
)