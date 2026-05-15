idf_component_register(
        SRCS
        "main.c"
        "lcd_display.c"
        "ship.c"
        "ready_queue.c"
        "HD44780.c"
        "config_parser.c"
        "channel.c"
        "scheduler.c"
        INCLUDE_DIRS "."
)

spiffs_create_partition_image(storage data FLASH_IN_PROJECT)