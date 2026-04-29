# ===== CONFIG =====
PORT ?= /dev/ttyUSB0
TARGET ?= esp32c6
IDF_PATH ?= $(HOME)/esp/esp-idf
IDF_EXPORT = export IDF_PATH=$(IDF_PATH) && export IDF_PATH_FORCE=1 && . $(IDF_PATH)/export.sh &&

# ===== EXPORT =====
export:
	$(IDF_EXPORT)

# ===== SET TARGET =====
set-target:
	$(IDF_EXPORT) idf.py set-target $(TARGET)

# ===== BUILD =====
idfbuild:
	$(IDF_EXPORT) idf.py build

# ===== FLASH =====
flash:
	$(IDF_EXPORT) idf.py flash

# ===== MONITOR =====
monitor:
	$(IDF_EXPORT) idf.py -p $(PORT) monitor

# ===== FLASH + MONITOR =====
run:
	$(IDF_EXPORT) idf.py -p $(PORT) flash monitor

# ===== CLEAN =====
clean:
	$(IDF_EXPORT) idf.py fullclean