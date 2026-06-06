#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef unsigned long long ull;
typedef long long ll;

#define N 1000000000LL

ull calcular_sumatoria(ll inicio, ll fin)
{
    ull suma=0;

    #pragma omp parallel for reduction(+:suma) schedule(dynamic)
    for(ll i=inicio; i<=fin; i++)
    {
        suma+= (ull)(i*i);
    }
    return suma;
}

int main(int argc, char *argv[])
{
    int rank, size;

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char hostname[256];
    //gethostname(hostname, 256);
    gethostname(hostname, sizeof(hostname));

    int num_threads = omp_get_max_threads();
    printf("Rank %d ejecutandose en %s usando %d hilos\n",
        rank,
        hostname,
        num_threads);

    if (rank == 0)
    {
        printf("\n=== SERVIDOR MPI ===\n");
        printf("Iniciando calculo distribuido\n");
        printf("Procesos MPI: %d\n", size);

        char imagenes[3][256] =
        {
            "images/dog.jpg",
            "images/dog.jpg",
            "images/dog.jpg"
        };

        //se tiene que enviar trabajo a cada nodo, ojo que no es por sockets
        /* Enviar una imagen a cada worker */
        for(int destino = 1; destino < size && destino <= 3; destino++)
        {
            MPI_Send(imagenes[destino - 1],
                     256,
                     MPI_CHAR,
                     destino,
                     0,
                     MPI_COMM_WORLD);

            printf("Servidor envio %s a rank %d\n",
                   imagenes[destino - 1],
                   destino);
        }
        printf("\nEsperando resultados...\n\n");

        /* Recibir resultados */
        for(int origen = 1; origen < size && origen <= 3; origen++)
        {
            int objetos_detectados;

            MPI_Recv(&objetos_detectados,
                     1,
                     MPI_INT,
                     origen,
                     0,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            printf("Servidor recibio %d objetos desde rank %d\n",
                objetos_detectados,
                origen);
            fflush(stdout);
        }

        printf("\nProcesamiento distribuido finalizado.\n");

        }
    else
    {
        char ruta_imagen[256];

        /* Recibir ruta de imagen */
        MPI_Recv(ruta_imagen,
                 256,
                 MPI_CHAR,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        printf("Rank %d recibio imagen: %s\n",
               rank,
               ruta_imagen);

        /*
         * Aqui ira Darknet posteriormente.
         * Por ahora simulamos procesamiento.
         */

        sleep(2);

        srand(rank);

        int objetos_detectados = rand() % 20 + 1;

        printf("Rank %d detecto %d objetos\n",
            rank,
            objetos_detectados);
        fflush(stdout);

        /* Enviar resultado al servidor */
        MPI_Send(&objetos_detectados,
                 1,
                 MPI_INT,
                 0,
                 0,
                 MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}