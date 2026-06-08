CC=mpicc

CHACHA_DIR=chacha_lib
DARKNET_DIR=../darknet

CFLAGS=-Wall -Wextra -O2 -I$(CHACHA_DIR) -I$(DARKNET_DIR)/include
LDFLAGS=-L$(CHACHA_DIR) -lchacha -lsodium $(DARKNET_DIR)/libdarknet.a -lm -pthread

TARGET=Servidor
SERVER=servidor
CLIENT=cliente
SRC=FirstServer.c detector.c
LIBCHACHA=$(CHACHA_DIR)/libchacha.a

IMG1=../images/dog.jpg
IMG2=../images/person.jpg
IMG3=../images/horses.jpg
IMAGES_DIR=../images

all: libchacha darknet_lib $(TARGET) $(SERVER) $(CLIENT)

$(TARGET): $(SRC) libchacha
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

$(SERVER): servidor.c detector.c libchacha
	$(CC) $(CFLAGS) servidor.c detector.c -o $(SERVER) $(LDFLAGS)

$(CLIENT): cliente.c detector.c libchacha
	$(CC) $(CFLAGS) cliente.c detector.c -o $(CLIENT) $(LDFLAGS)

run:
	mpirun -np 4 ./$(TARGET) $(IMG1) $(IMG2) $(IMG3)

run-server-client:
	mpirun -np 1 ./$(SERVER) $(IMG1) : -np 1 ./$(CLIENT)

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
	rm -f $(TARGET) $(SERVER) $(CLIENT) $(CHACHA_DIR)/chacha.o $(LIBCHACHA)
