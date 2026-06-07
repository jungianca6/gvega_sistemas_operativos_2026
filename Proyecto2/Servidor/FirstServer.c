#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <omp.h>
#include <sodium.h>
#include "chacha.h"  // API de libchacha para cifrado/desifrado XChaCha20-Poly1305
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

    int num_threads = omp_get_max_threads();
    printf("Rank %d ejecutandose en %s usando %d hilos\n",
           rank,
           hostname,
           num_threads);

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

        /* Generar y compartir la clave de cifrado con los nodos */
        unsigned char key[CHACHA_KEYBYTES];
        randombytes_buf(key, CHACHA_KEYBYTES);
        MPI_Bcast(key, CHACHA_KEYBYTES, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

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

            unsigned long long plain_len = (unsigned long long)cantidad * MAX_WORD;
            unsigned char nonce[CHACHA_NONCEBYTES];
            unsigned char *ciphertext = malloc(plain_len + crypto_aead_xchacha20poly1305_ietf_ABYTES);
            unsigned long long cipher_len = 0;

            if(ciphertext == NULL)
            {
                printf("Error de memoria\n");
                MPI_Finalize();
                return 1;
            }

            if(chacha_encrypt(key,
                              (const unsigned char *)(palabras + inicio),
                              plain_len,
                              nonce,
                              ciphertext,
                              &cipher_len) != 0)
            {
                printf("Error cifrando palabras\n");
                free(ciphertext);
                MPI_Finalize();
                return 1;
            }

            MPI_Send(
                &cantidad,
                1,
                MPI_INT,
                destino,
                0,
                MPI_COMM_WORLD);

            MPI_Send(
                &cipher_len,
                1,
                MPI_UNSIGNED_LONG_LONG,
                destino,
                0,
                MPI_COMM_WORLD);

            MPI_Send(
                nonce,
                CHACHA_NONCEBYTES,
                MPI_UNSIGNED_CHAR,
                destino,
                0,
                MPI_COMM_WORLD);

            MPI_Send(
                ciphertext,
                cipher_len,
                MPI_UNSIGNED_CHAR,
                destino,
                0,
                MPI_COMM_WORLD);

            printf(
                "Maestro envio %d palabras cifradas a rank %d\n",
                cantidad,
                destino);

            free(ciphertext);
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
            unsigned long long cipher_len2 = 0;
            MPI_Recv(
                &cipher_len2,
                1,
                MPI_UNSIGNED_LONG_LONG,
                origen,
                0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            unsigned char nonce2[CHACHA_NONCEBYTES];
            MPI_Recv(
                nonce2,
                CHACHA_NONCEBYTES,
                MPI_UNSIGNED_CHAR,
                origen,
                0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            unsigned char *ciphertext2 = malloc(cipher_len2);
            if(ciphertext2 == NULL)
            {
                printf("Error de memoria al recibir tabla de rank %d\n", origen);
                MPI_Finalize();
                return 1;
            }

            MPI_Recv(
                ciphertext2,
                cipher_len2,
                MPI_UNSIGNED_CHAR,
                origen,
                0,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            WordCount tabla_worker[MAX_WORDS];
            unsigned long long decrypted_len2 = 0;

            if(chacha_decrypt(key,
                              ciphertext2,
                              cipher_len2,
                              nonce2,
                              (unsigned char *)tabla_worker,
                              &decrypted_len2) != 0)
            {
                printf("Error descifrando tabla del rank %d\n", origen);
                free(ciphertext2);
                MPI_Finalize();
                return 1;
            }

            free(ciphertext2);

            int unicas = (int)(decrypted_len2 / sizeof(WordCount));

            printf(
                "Recibida tabla cifrada desde rank %d (%d palabras unicas)\n",
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
        unsigned char key[CHACHA_KEYBYTES];
        MPI_Bcast(key, CHACHA_KEYBYTES, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

        int cantidad;

        MPI_Recv(
            &cantidad,
            1,
            MPI_INT,
            0,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

        unsigned long long cipher_len = 0;
        MPI_Recv(
            &cipher_len,
            1,
            MPI_UNSIGNED_LONG_LONG,
            0,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

        unsigned char nonce[CHACHA_NONCEBYTES];
        MPI_Recv(
            nonce,
            CHACHA_NONCEBYTES,
            MPI_UNSIGNED_CHAR,
            0,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

        unsigned char *ciphertext = malloc(cipher_len);
        if(ciphertext == NULL)
        {
            printf("Error de memoria\n");
            MPI_Finalize();
            return 1;
        }

        MPI_Recv(
            ciphertext,
            cipher_len,
            MPI_UNSIGNED_CHAR,
            0,
            0,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

        char (*palabras_locales)[MAX_WORD] = malloc(cantidad * MAX_WORD);
        if(palabras_locales == NULL)
        {
            printf("Error de memoria\n");
            free(ciphertext);
            MPI_Finalize();
            return 1;
        }

        unsigned long long decrypted_len = 0;
        if(chacha_decrypt(key,
                         ciphertext,
                         cipher_len,
                         nonce,
                         (unsigned char *)palabras_locales,
                         &decrypted_len) != 0)
        {
            printf("Error descifrando palabras en rank %d\n", rank);
            free(ciphertext);
            free(palabras_locales);
            MPI_Finalize();
            return 1;
        }

        free(ciphertext);

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

        unsigned long long plaintext_len = unicas * sizeof(WordCount);
        unsigned char nonce2[CHACHA_NONCEBYTES];
        unsigned long long cipher_len2 = 0;
        unsigned char *ciphertext2 = malloc(plaintext_len + crypto_aead_xchacha20poly1305_ietf_ABYTES);
        if(ciphertext2 == NULL)
        {
            printf("Error de memoria\n");
            free(palabras_locales);
            MPI_Finalize();
            return 1;
        }

        if(chacha_encrypt(key,
                          (const unsigned char *)tabla_local,
                          plaintext_len,
                          nonce2,
                          ciphertext2,
                          &cipher_len2) != 0)
        {
            printf("Error cifrando tabla local en rank %d\n", rank);
            free(ciphertext2);
            free(palabras_locales);
            MPI_Finalize();
            return 1;
        }

        MPI_Send(
            &cipher_len2,
            1,
            MPI_UNSIGNED_LONG_LONG,
            0,
            0,
            MPI_COMM_WORLD);

        MPI_Send(
            nonce2,
            CHACHA_NONCEBYTES,
            MPI_UNSIGNED_CHAR,
            0,
            0,
            MPI_COMM_WORLD);

        MPI_Send(
            ciphertext2,
            cipher_len2,
            MPI_UNSIGNED_CHAR,
            0,
            0,
            MPI_COMM_WORLD);

        free(ciphertext2);
        free(palabras_locales);
    }

    MPI_Finalize();

    return 0;
}

