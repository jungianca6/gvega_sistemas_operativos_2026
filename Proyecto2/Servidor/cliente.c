#include <mpi.h>
#include <stdio.h>

#define SERVIDOR 0

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank;

    MPI_Comm_rank(
        MPI_COMM_WORLD,
        &rank);

    if(rank == 2)
    {
        char imagen[256] =
            "../images/dog.jpg";

        printf(
            "Cliente enviando imagen: %s\n",
            imagen);

        MPI_Send(
            imagen,
            256,
            MPI_CHAR,
            SERVIDOR,
            0,
            MPI_COMM_WORLD);

        int resultado;

        MPI_Recv(
            &resultado,
            1,
            MPI_INT,
            SERVIDOR,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

        printf(
            "Cliente recibio resultado: %d objetos\n",
            resultado);
    }

    MPI_Finalize();

    return 0;
}