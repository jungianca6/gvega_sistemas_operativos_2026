CC=mpicc

CFLAGS=-Wall -Wextra -O2 -fopenmp

TARGET=Servidor
TEXTO=texto.txt
SRC=FirstServer.c word_counter.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run:
	mpirun -np 4 ./$(TARGET) $(TEXTO)

debug:
	$(CC) -g $(CFLAGS) $(SRC) -o $(TARGET)
clean:
	rm -f $(TARGET)