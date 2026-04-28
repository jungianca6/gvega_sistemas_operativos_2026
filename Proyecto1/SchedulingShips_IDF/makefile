# ===== CONFIG =====
PORT ?= /dev/ttyUSB0
TARGET ?= esp32c6
IDF_PATH ?= ~/esp/esp-idf

# ===== EXPORT ENV =====
export:
	. $(IDF_PATH)/export.sh

# ===== SET TARGET =====
set-target:
	. $(IDF_PATH)/export.sh && idf.py set-target $(TARGET)

# ===== BUILD =====
build:
	. $(IDF_PATH)/export.sh && idf.py build

# ===== FLASH =====
flash:
	. $(IDF_PATH)/export.sh && idf.py -p $(PORT) flash

# ===== MONITOR =====
monitor:
	. $(IDF_PATH)/export.sh && idf.py -p $(PORT) monitor

# ===== FLASH + MONITOR =====
run:
	. $(IDF_PATH)/export.sh && idf.py -p $(PORT) flash monitor

# ===== CLEAN =====
clean:
	. $(IDF_PATH)/export.sh && idf.py fullclean