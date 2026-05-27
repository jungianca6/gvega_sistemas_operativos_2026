#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef unsigned long long ull;
typedef long long ll;

#define N 1000000LL

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
    gethostname(hostname, 256);

    int num_threads = omp_get_max_threads();
    printf("Rank %d ejecutandose en %s usando %d hilos\n",
        rank,
        hostname,
        num_threads);

    if (rank == 0)
    {
        printf("Iniciando calculo distribuido");
        printf("Procesos MPI: %d\n", size);

        ll trabajo_por_nodo = N / size;
        ull resultado_final = 0;

        //se tiene que enviar trabajo a cada nodo, ojo que no es por sockets
        for (int destino = 1; destino < size; destino++)
        {
            ll rango [2];

            rango [0] = destino * trabajo_por_nodo +1;

            if (destino == size -1)
                rango[1]=N;
            else
                rango[1] = (destino+1) * trabajo_por_nodo;
            //No utilizar scatter ni ninguna que tenga que ver con broadcast
            MPI_Send(rango,
                2,
                MPI_LONG_LONG,
                destino,
                0,
                MPI_COMM_WORLD);
            printf("Maestro envio rango [%lld - %lld] a rank %d\n",
                rango[0],
                rango[1],
                destino);
        }
        // El maestro también trabaja
        ll mi_inicio = 1;
        ll mi_fin = trabajo_por_nodo;

        ull suma_local = calcular_sumatoria(mi_inicio, mi_fin);

        resultado_final += suma_local;

        printf("Maestro calculo local: %llu\n", suma_local);

        // Recibir resultados de workers
        for(int origen = 1; origen < size; origen++)
        {
            ull parcial;

            MPI_Recv(&parcial,
                     1,
                     MPI_UNSIGNED_LONG_LONG,
                     origen,
                     0,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            printf("Maestro recibio %llu desde rank %d\n",
                   parcial,
                   origen);

            resultado_final += parcial;
        }
        printf("Resultado final = %llu\n", resultado_final);
    }
    //Este seria el codigo que ejecutarian los esclavos
    else
    {
        ll rango[2];

        //Aqui se reciben los datos
        MPI_Recv(rango,
            2,
            MPI_LONG_LONG,
            0,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

        ll inicio = rango[0];
        ll fin = rango [1];

        printf("Rank %d procesando [%lld - %lld]\n",
               rank,
               inicio,
               fin);

        ull suma_parcial = calcular_sumatoria(inicio, fin);

        printf("Rank %d calculo %llu\n",
               rank,
               suma_parcial);

        //Aqui se devuelve el resultado
        MPI_Send(&suma_parcial,
            1,
            MPI_UNSIGNED_LONG_LONG,
            0,
            0,
            MPI_COMM_WORLD);
    }
    MPI_Finalize();
    return 0;
}
