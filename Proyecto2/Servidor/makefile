CC=mpicc

CHACHA_DIR=chacha_lib
DARKNET_DIR=../darknet

CFLAGS=-Wall -Wextra -O2 -fopenmp -I$(CHACHA_DIR) -I$(DARKNET_DIR)/include
LDFLAGS=-L$(CHACHA_DIR) -lchacha -lsodium $(DARKNET_DIR)/libdarknet.a -lm -pthread

TARGET=Servidor
SRC=FirstServer.c detector.c
LIBCHACHA=$(CHACHA_DIR)/libchacha.a

IMG1=../darknet/data/dog.jpg
IMG2=../darknet/data/person.jpg
IMG3=../darknet/data/horses.jpg

all: libchacha darknet_lib $(TARGET)

$(TARGET): $(SRC) libchacha
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

run:
	mpirun -np 4 ./$(TARGET) $(IMG1) $(IMG2) $(IMG3)

libchacha: $(LIBCHACHA)

$(CHACHA_DIR)/chacha.o: $(CHACHA_DIR)/chacha.c $(CHACHA_DIR)/chacha.h
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBCHACHA): $(CHACHA_DIR)/chacha.o
	ar rcs $@ $^

darknet_lib:
	$(MAKE) -C $(DARKNET_DIR) libdarknet.a

clean:
	rm -f $(TARGET) $(CHACHA_DIR)/chacha.o $(LIBCHACHA)
