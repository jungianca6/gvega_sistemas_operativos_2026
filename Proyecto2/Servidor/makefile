CC=mpicc

CHACHA_DIR=chacha_lib
DARKNET_DIR=../darknet

CFLAGS=-Wall -Wextra -O2 -fopenmp -I$(CHACHA_DIR) -I$(DARKNET_DIR)/include

# MPI library path for explicit linking
MPI_LIBDIR=/opt/intel/oneapi/mpi/2021.17/lib

# Ensure RPATH is properly formatted as a single linker flag
LDFLAGS=-L$(CHACHA_DIR) -Wl,-L$(MPI_LIBDIR) -lchacha -lsodium -lmpi $(DARKNET_DIR)/libdarknet.a -lm -pthread -Wl,-rpath=$(MPI_LIBDIR)

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
	@bash run_all_batches.sh

libchacha: $(LIBCHACHA)

$(CHACHA_DIR)/chacha.o: $(CHACHA_DIR)/chacha.c $(CHACHA_DIR)/chacha.h
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBCHACHA): $(CHACHA_DIR)/chacha.o
	ar rcs $@ $^

darknet_lib:
	$(MAKE) -C $(DARKNET_DIR) libdarknet.a

clean:
	rm -f $(TARGET) $(CHACHA_DIR)/chacha.o $(LIBCHACHA)
