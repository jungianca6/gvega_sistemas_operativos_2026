#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "ship.h"
#include "channel.h"
#include "ready_queue.h"
#include "esp_err.h"
#include "scheduler.h"

#define MAX_PRESET_SHIPS 20

typedef struct {
    FlowControl flow_control;
    int channel_length;
    int standard_speed;
    int fishing_speed;
    int patrol_speed;
    ShipType queue_left[MAX_PRESET_SHIPS];
    int queue_left_count;
    ShipType queue_right[MAX_PRESET_SHIPS];
    int queue_right_count;
    int sign_duration;
    int parameter_w;
    int quantum_rr;
    SchedulerType scheduler;
} AppConfig;

/**
 * @brief Parses the configuration file from the given path.
 * 
 * @param path The path to the config file (e.g. "/spiffs/config.txt")
 * @param config Pointer to the AppConfig struct to fill.
 * @return esp_err_t ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t parseConfigFile(const char* path, AppConfig* config);

/**
 * @brief Populates a QueueShip with ships based on the parsed configuration.
 * 
 * @param q Pointer to the QueueShip.
 * @param types Array of ShipTypes.
 * @param count Number of ships to add.
 * @param dir Direction for the new ships.
 * @param speeds Pointer to speeds array (ordered by ShipType index: STANDARD, FISHING, PATROL)
 * @param id_start Pointer to the current boat ID counter.
 */
void populate_queue_from_config(QueueShip* q, ShipType* types, int count,
    Direction dir, int* speeds, int* id_start,
    SchedulerType scheduler, void (*create_task)(void*));

#endif // CONFIG_PARSER_H
