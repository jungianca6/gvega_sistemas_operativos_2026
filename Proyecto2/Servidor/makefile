CC=mpicc

CHACHA_DIR=chacha_lib
DARKNET_DIR=../darknet
WOLFPLOTTER_DIR=../libAndDriver
WOLFPLOTTER_LIB=$(WOLFPLOTTER_DIR)/libwolfPlotter.a

CFLAGS=-Wall -Wextra -O2 -I$(CHACHA_DIR) -I$(DARKNET_DIR)/include -I$(WOLFPLOTTER_DIR)
LDFLAGS=-L$(CHACHA_DIR) -lchacha -lsodium $(DARKNET_DIR)/libdarknet.a \
        $(WOLFPLOTTER_LIB) -lm -pthread

TARGET=servidor
SERVER=servidor
CLIENT=cliente
SRC=servidor.c detector.c
LIBCHACHA=$(CHACHA_DIR)/libchacha.a

IMG1=../images/dog.jpg
IMG2=../images/person.jpg
IMG3=../images/horses.jpg
IMAGES_DIR=../images

<<<<<<< HEAD
CLIENT_HOST=192.168.0.100
SERVER_HOST=192.168.0.101
WORKER_HOST=192.168.0.100
WORKER2=172.18.189.69
=======
MASTER=192.168.100.58
WORKER1=192.168.100.20
>>>>>>> integration

all: libchacha darknet_lib wolfplotter_lib $(TARGET) $(CLIENT)

$(TARGET): $(SRC) libchacha
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

$(CLIENT): cliente.c detector.c libchacha
	$(CC) $(CFLAGS) cliente.c detector.c -o $(CLIENT) $(LDFLAGS)

run:
	mpirun -np 4 ./$(TARGET) $(IMG1) $(IMG2) $(IMG3)

run_local:
	mpirun -np 1 ./$(CLIENT) : -np 4 ./$(SERVER)

run_local2:
	mpirun --oversubscribe -np 1 ./$(CLIENT) : -np 4 ./$(SERVER)

run-server-client:
	mpirun -np 1 ./$(SERVER) $(IMG1) : -np 1 ./$(CLIENT)

run-distributed:
	mpirun --oversubscribe \
<<<<<<< HEAD
	-host $(CLIENT_HOST):1 -np 1 ./cliente \
=======
	-host $(WORKER1):1 -np 1 ./cliente \
>>>>>>> integration
	: \
	-host $(SERVER_HOST):1 -np 1 ./servidor \
	: \
<<<<<<< HEAD
	-host $(WORKER_HOST):3 -np 3 ./servidor 
=======
	-host $(WORKER1):3 -np 3 ./servidor
>>>>>>> integration

run-all:
	@bash run_all_batches.sh

libchacha: $(LIBCHACHA)

$(CHACHA_DIR)/chacha.o: $(CHACHA_DIR)/chacha.c $(CHACHA_DIR)/chacha.h
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBCHACHA): $(CHACHA_DIR)/chacha.o
	ar rcs $@ $^

darknet_lib:
	$(MAKE) -C $(DARKNET_DIR) libdarknet.a

wolfplotter_lib: $(WOLFPLOTTER_LIB)

$(WOLFPLOTTER_LIB):
	$(MAKE) -C $(WOLFPLOTTER_DIR) lib

sync:
	rsync -avz --exclude '*.o' --exclude '*.a' \
	--exclude 'servidor' --exclude 'cliente' \
	--exclude 'mpi_hello' --exclude 'test_chacha_mpi' \
	./ smz19@$(MASTER):~/Desktop/gvega_sistemas_operativos_2026/Proyecto2/Servidor/
	rsync -avz --exclude '*.o' --exclude '*.a' --exclude '*.ko' \
	--exclude 'blink_test' --exclude 'wolfLibTest' \
	$(WOLFPLOTTER_DIR)/ smz19@$(MASTER):~/Desktop/gvega_sistemas_operativos_2026/Proyecto2/libAndDriver/

driver-load:
	$(MAKE) -C $(WOLFPLOTTER_DIR) load

driver-unload:
	$(MAKE) -C $(WOLFPLOTTER_DIR) unload

driver-verify:
	$(MAKE) -C $(WOLFPLOTTER_DIR) verify

clean:
	rm -f $(TARGET) $(SERVER) $(CLIENT) $(CHACHA_DIR)/chacha.o $(LIBCHACHA)
	rm -rf ../result/*
	rm -rf ../receive/*
	$(MAKE) -C $(WOLFPLOTTER_DIR) clean
