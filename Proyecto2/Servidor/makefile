CC=mpicc

CHACHA_DIR=chacha_lib
DARKNET_DIR=../darknet

CFLAGS=-Wall -Wextra -O2 -fopenmp -I$(CHACHA_DIR) -I$(DARKNET_DIR)/include
LDFLAGS=-L$(CHACHA_DIR) -lchacha -lsodium $(DARKNET_DIR)/libdarknet.a -lm -pthread

TARGET=Servidor
SRC=FirstServer.c detector.c
LIBCHACHA=$(CHACHA_DIR)/libchacha.a

IMG1=../images/dog.jpg
IMG2=../images/person.jpg
IMG3=../images/horses.jpg
IMAGES_DIR=../images

all: libchacha darknet_lib $(TARGET)

$(TARGET): $(SRC) libchacha
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

run:
	mpirun -np 4 ./$(TARGET) $(IMG1) $(IMG2) $(IMG3)

run-all:
	@bash -lc 'mpi_cmd=$$(which mpirun 2>/dev/null || which mpiexec 2>/dev/null || true); if [ -z "$$mpi_cmd" ]; then echo "mpirun/mpiexec no se encuentra en PATH."; exit 1; fi; mpi_ld_dirs=$$(mpicc -show 2>/dev/null | tr " " "\n" | grep "^-L" | sed "s/^-L//" | tr "\n" ":" | sed "s/:$$//"); if [ -z "$$mpi_ld_dirs" ]; then mpi_ld_dirs=$$(mpicc --showme:link 2>/dev/null | tr " " "\n" | grep "^-L" | sed "s/^-L//" | tr "\n" ":" | sed "s/:$$//" || true); fi; if [ -n "$$mpi_ld_dirs" ]; then export LD_LIBRARY_PATH="$$mpi_ld_dirs:$${LD_LIBRARY_PATH}"; fi; imgs=(../images/*.jpg); n=$${#imgs[@]}; if [ $$n -lt 3 ]; then echo "Necesitas al menos 3 imágenes en ../images"; exit 1; fi; for ((i=0; i<n; i+=3)); do batch=( "$${imgs[@]:i:3}" ); if [ "$${#batch[@]}" -lt 3 ]; then echo "Lote incompleto con $${#batch[@]} imágenes omitido."; break; fi; echo "=== Lote $$((i/3+1)): $${batch[*]} ==="; "$$mpi_cmd" -np 4 ./$(TARGET) "$${batch[@]}"; done'

libchacha: $(LIBCHACHA)

$(CHACHA_DIR)/chacha.o: $(CHACHA_DIR)/chacha.c $(CHACHA_DIR)/chacha.h
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBCHACHA): $(CHACHA_DIR)/chacha.o
	ar rcs $@ $^

darknet_lib:
	$(MAKE) -C $(DARKNET_DIR) libdarknet.a

clean:
	rm -f $(TARGET) $(CHACHA_DIR)/chacha.o $(LIBCHACHA)
