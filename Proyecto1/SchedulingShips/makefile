# ─── Configuración ───────────────────────────────────────────
IDF_PATH     ?= $(HOME)/esp/esp-idf
IDF_EXPORT    = . $(IDF_PATH)/export.sh
TARGET        = esp32c6
PORT         ?= /dev/ttyUSB0   # ajustar si el puerto es distinto

# ─── Targets ─────────────────────────────────────────────────
.PHONY: all build flash monitor clean menuconfig

all: build

build:
	@bash -c "$(IDF_EXPORT) && idf.py build"

flash:
	@bash -c "$(IDF_EXPORT) && idf.py -p $(PORT) flash"

monitor:
	@bash -c "$(IDF_EXPORT) && idf.py -p $(PORT) monitor"

flash-monitor:
	@bash -c "$(IDF_EXPORT) && idf.py -p $(PORT) flash monitor"

menuconfig:
	@bash -c "$(IDF_EXPORT) && idf.py menuconfig"

set-target:
	@bash -c "$(IDF_EXPORT) && idf.py set-target $(TARGET)"

clean:
	@bash -c "$(IDF_EXPORT) && idf.py fullclean"