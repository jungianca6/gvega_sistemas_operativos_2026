#include "config_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"

static const char *TAG = "CONFIG_PARSER";

static void trim(char *s) {
    char *p = s;
    int l = strlen(p);
    while (isspace(p[l - 1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p, --l;
    memmove(s, p, l + 1);
}

static ShipType charToShipType(char c) {
    switch (toupper((unsigned char)c)) {
        case 'F': return FISHING;
        case 'P': return PATROL;
        case 'S':
        default: return STANDARD;
    }
}

esp_err_t parseConfigFile(const char* path, AppConfig* config) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path);
        return ESP_FAIL;
    }

    char line[128];
    memset(config, 0, sizeof(AppConfig));

    while (fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;

        *colon = '\0';
        char *key = line;
        char *value = colon + 1;

        trim(key);
        trim(value);

        if (strcmp(key, "FlowControl") == 0) {
            if (strcasecmp(value, "Fairness") == 0) config->flow_control = FAIRNESS;
            else if (strcasecmp(value, "Sign") == 0) config->flow_control = SIGN;
            else config->flow_control = TICO;
        } else if (strcmp(key, "ChannelLength") == 0) {
            config->channel_length = atoi(value);
        } else if (strcmp(key, "Standard Speed") == 0) {
            config->standard_speed = atoi(value);
        } else if (strcmp(key, "FishingSpeed") == 0) {
            config->fishing_speed = atoi(value);
        } else if (strcmp(key, "PatrolSpeed") == 0) {
            config->patrol_speed = atoi(value);
        } else if (strcmp(key, "SignDuration") == 0) {
            config->sign_duration = atoi(value);
        } else if (strcmp(key, "ParameterW") == 0) {
            config->parameter_w = atoi(value);
        } else if (strcmp(key, "QuantumRR") == 0) {
            config->quantum_rr = atoi(value);
        } else if (strcmp(key, "QueueLeft") == 0 || strcmp(key, "QueueRight") == 0) {
            int is_left = (strcmp(key, "QueueLeft") == 0);
            int count = 0;
            char *token = strtok(value, ",");
            while (token && count < MAX_PRESET_SHIPS) {
                trim(token);
                if (strlen(token) > 0) {
                    if (is_left) config->queue_left[count++] = charToShipType(token[0]);
                    else config->queue_right[count++] = charToShipType(token[0]);
                }
                token = strtok(NULL, ",");
            }
            if (is_left) config->queue_left_count = count;
            else config->queue_right_count = count;
        } else if (strcmp(key, "Scheduler") == 0) {
            if (strcasecmp(value, "FCFS") == 0)
                config->scheduler = SCHEDULER_FCFS;

            else if (strcasecmp(value, "RR") == 0)
                config->scheduler = SCHEDULER_RR;

            else if (strcasecmp(value, "PRIORITY") == 0)
                config->scheduler = SCHEDULER_PRIORITY;

            else if (strcasecmp(value, "SJF") == 0)
                config->scheduler = SCHEDULER_SJF;

            else if (strcasecmp(value, "STRN") == 0)
                config->scheduler = SCHEDULER_STRN;

            else if (strcasecmp(value, "EDF") == 0)
                config->scheduler = SCHEDULER_EDF;
        }
    }

    fclose(f);
    ESP_LOGI(TAG, "Configuration loaded successfully from %s", path);
    return ESP_OK;
}

void populate_queue_from_config(QueueShip* q, ShipType* types, int count,
    Direction dir, int* speeds, int* id_start,
    SchedulerType scheduler, void (*create_task)(void*)) {
    for (int i = 0; i < count; i++) {
        Ship* barco = malloc(sizeof(Ship));
        if (!barco) break;
        
        ShipType type = types[i];
        int speed = speeds[type];

        int burst    = (type == PATROL) ? 3  : (type == FISHING) ? 6  : 10;
        int priority = (type == PATROL) ? 0  : (type == FISHING) ? 1  : 2;
        int deadline = (type == PATROL) ? 5  : (type == FISHING) ? 15 : 30;
        inicializar_barco(barco, (*id_start)++, type, dir, speed, burst, priority, deadline);
        if (scheduler_enqueue_ordered(q, barco, scheduler)) {
            if (create_task) {
                create_task(barco);
            }
        } else {
            vSemaphoreDelete(barco->sem);
            free(barco);
            break;
        }
    }
}
