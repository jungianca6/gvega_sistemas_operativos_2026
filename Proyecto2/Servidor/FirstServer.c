#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "word_counter.h"
#define MAX_FILE_WORDS 100000



int main(int argc, char *argv[])
{
    int rank;
    int size;

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char hostname[256];

    gethostname(hostname, sizeof(hostname));

    printf("Rank %d ejecutandose en %s\n",
           rank,
           hostname);

    if(argc < 2)
    {
        if(rank == 0)
        {
            printf("Uso:\n");
            printf("./Servidor archivo.txt\n");
        }

        MPI_Finalize();
        return 1;
    }

    if(rank == 0)
    {
        FILE *file = fopen(argv[1], "r");

        if(file == NULL)
        {
            printf("No se pudo abrir el archivo\n");

            MPI_Finalize();
            return 1;
        }

        char texto[50000];

        size_t leidos =
            fread(
                texto,
                sizeof(char),
                sizeof(texto)-1,
                file);

        texto[leidos] = '\0';

        fclose(file);

        char palabras[MAX_FILE_WORDS][MAX_WORD];

        int total_palabras = 0;

        char *token =
            strtok(
                texto,
                " \n\t\r");

        while(token != NULL)
        {
            char palabra_limpia[MAX_WORD];

            strncpy(
                palabra_limpia,
                token,
                MAX_WORD - 1);

            palabra_limpia[MAX_WORD - 1] = '\0';

            normalizar_palabra(
                palabra_limpia);

            if(strlen(palabra_limpia) > 0)
            {
                strcpy(
                    palabras[total_palabras],
                    palabra_limpia);

                total_palabras++;
            }

            token =
                strtok(
                    NULL,
                    " \n\t\r");
        }
        printf(
            "Total palabras: %d\n",
            total_palabras);

        int palabras_por_nodo =
            total_palabras / size;

        WordCount tabla_global[MAX_WORDS];
        int global_count = 0;

        for(int destino = 1;
            destino < size;
            destino++)
        {
            int inicio =
                destino *
                palabras_por_nodo;

            int cantidad;

            if(destino == size - 1)
            {
                cantidad =
                    total_palabras -
                    inicio;
            }
            else
            {
                cantidad =
                    palabras_por_nodo;
            }

            MPI_Send(
                &cantidad,
                1,
                MPI_INT,
                destino,
                0,
                MPI_COMM_WORLD);

            MPI_Send(
                palabras + inicio,
                cantidad * MAX_WORD,
                MPI_CHAR,
                destino,
                0,
                MPI_COMM_WORLD);

            printf(
                "Maestro envio %d palabras a rank %d\n",
                cantidad,
                destino);
        }

        int mi_cantidad =
            palabras_por_nodo;

        WordCount tabla_local[MAX_WORDS];

        int unicas_local =
            contar_palabras(
                palabras,
                mi_cantidad,
                tabla_local);

        fusionar_tablas(
            tabla_global,
            &global_count,
            tabla_local,
            unicas_local);

        for(int origen = 1;
            origen < size;
            origen++)
        {
            int unicas;

            MPI_Recv(
                &unicas,
                1,
                MPI_INT,
                origen,
                0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            WordCount tabla_worker[MAX_WORDS];

            MPI_Recv(
                tabla_worker,
                unicas *
                sizeof(WordCount),
                MPI_BYTE,
                origen,
                0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            printf(
                "Recibida tabla desde rank %d (%d palabras unicas)\n",
                origen,
                unicas);

            fusionar_tablas(
                tabla_global,
                &global_count,
                tabla_worker,
                unicas);
        }

        WordCount ganador =
            buscar_maximo(
                tabla_global,
                global_count);

        printf("\n");
        printf("=================================\n");
        printf("RESULTADO GLOBAL\n");
        printf("=================================\n");

        for(int i = 0;
            i < global_count;
            i++)
        {
            printf(
                "%s = %d\n",
                tabla_global[i].word,
                tabla_global[i].count);
        }

        printf("\n");
        printf("PALABRA GANADORA\n");
        printf("%s (%d)\n",
               ganador.word,
               ganador.count);

        printf("=================================\n");
    }
    else
    {
        int cantidad;

        MPI_Recv(
            &cantidad,
            1,
            MPI_INT,
            0,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

        char palabras_locales[MAX_FILE_WORDS][MAX_WORD];

        MPI_Recv(
            palabras_locales,
            cantidad * MAX_WORD,
            MPI_CHAR,
            0,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

        WordCount tabla_local[MAX_WORDS];

        int unicas =
            contar_palabras(
                palabras_locales,
                cantidad,
                tabla_local);

        printf(
            "Rank %d proceso %d palabras (%d unicas)\n",
            rank,
            cantidad,
            unicas);

        MPI_Send(
            &unicas,
            1,
            MPI_INT,
            0,
            0,
            MPI_COMM_WORLD);

        MPI_Send(
            tabla_local,
            unicas *
            sizeof(WordCount),
            MPI_BYTE,
            0,
            0,
            MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}

