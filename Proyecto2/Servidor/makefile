CC=mpicc

CFLAGS=-Wall -Wextra -O2 -fopenmp

TARGET=Servidor

SRC=main.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run:
	mpirun -np 4 ./$(TARGET)

debug:
	$(CC) -g $(CFLAGS) $(SRC) -o $(TARGET)
clean:
	rm -f $(TARGET)