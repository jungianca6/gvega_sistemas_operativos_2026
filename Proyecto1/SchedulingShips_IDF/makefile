# ===== CONFIG =====
PORT ?= /dev/ttyUSB0
TARGET ?= esp32c6
# Try to get IDF_PATH from environment, otherwise use default
IDF_PATH ?= $(shell echo $$IDF_PATH)
ifeq ($(IDF_PATH),)
    IDF_PATH = $(HOME)/esp/esp-idf
endif
# Use a more robust export string
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

# ===== RUN GUI =====
gui:
	python3 serial_gui.py

# ===== CLEAN =====
clean:
	$(IDF_EXPORT) idf.py fullclean