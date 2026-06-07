CC=mpicc

CFLAGS=-Wall -Wextra -O2 -fopenmp

TARGET=Servidor
TEXTO=texto.txt
SRC=FirstServer.c word_counter.c
CHACHA_SRC=test_chacha_mpi.c
CHACHA_TARGET=chacha_test
CHACHA_DIR=chacha_lib
LIBCHACHA=$(CHACHA_DIR)/libchacha.a
CHACHATEST_SRC=$(CHACHA_DIR)/chacha_lib_test_mpi.c
CHACHATEST_BIN=chacha_lib_test

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run:
	mpirun -np 4 ./$(TARGET) $(TEXTO)

chacha_test: $(CHACHA_SRC)
	$(CC) $(CFLAGS) $< -o $(CHACHA_TARGET) -lsodium

run_chacha:
	mpirun -np 2 ./$(CHACHA_TARGET)

libchacha: $(LIBCHACHA)

$(CHACHA_DIR)/chacha.o: $(CHACHA_DIR)/chacha.c $(CHACHA_DIR)/chacha.h
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBCHACHA): $(CHACHA_DIR)/chacha.o
	ar rcs $@ $^

chacha_lib_test: $(CHACHATEST_SRC) libchacha
	$(CC) $(CFLAGS) $(CHACHATEST_SRC) -L$(CHACHA_DIR) -lchacha -lsodium -o $(CHACHATEST_BIN)

run_chacha_lib:
	mpirun -np 2 ./$(CHACHATEST_BIN)

debug:
	$(CC) -g $(CFLAGS) $(SRC) -o $(TARGET)
clean:
	rm -f $(TARGET) $(CHACHA_TARGET)