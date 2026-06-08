#include <mpi.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chacha.h"

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 2)
    {
        if (rank == 0)
        {
            fprintf(stderr, "Use mpirun -np 2 ./chacha_server_text\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (sodium_init() < 0)
    {
        if (rank == 0)
        {
            fprintf(stderr, "Error iniciando libsodium\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (rank == 0)
    {
        unsigned char key[CHACHA_KEYBYTES];
        randombytes_buf(key, CHACHA_KEYBYTES);

        const char *mensaje = "Hola desde rank 0";
        unsigned long long msg_len = strlen(mensaje) + 1;

        unsigned char nonce[CHACHA_NONCEBYTES];
        unsigned char *ciphertext = malloc(msg_len + crypto_aead_xchacha20poly1305_ietf_ABYTES);
        unsigned long long cipher_len = 0;

        if (ciphertext == NULL)
        {
            perror("malloc");
            MPI_Finalize();
            return 1;
        }

        if (chacha_encrypt(key,
                           (const unsigned char *)mensaje,
                           msg_len,
                           nonce,
                           ciphertext,
                           &cipher_len) != 0)
        {
            fprintf(stderr, "Error cifrando el mensaje\n");
            free(ciphertext);
            MPI_Finalize();
            return 1;
        }

        MPI_Send(key, CHACHA_KEYBYTES, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);
        MPI_Send(&cipher_len, 1, MPI_UNSIGNED_LONG_LONG, 1, 0, MPI_COMM_WORLD);
        MPI_Send(nonce, CHACHA_NONCEBYTES, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);
        MPI_Send(ciphertext, cipher_len, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);

        printf("Rank 0 envió texto cifrado al servidor\n");
        free(ciphertext);
    }
    else
    {
        unsigned char key[CHACHA_KEYBYTES];
        unsigned long long cipher_len = 0;
        unsigned char nonce[CHACHA_NONCEBYTES];

        MPI_Recv(key, CHACHA_KEYBYTES, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&cipher_len, 1, MPI_UNSIGNED_LONG_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(nonce, CHACHA_NONCEBYTES, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        unsigned char *ciphertext = malloc(cipher_len);
        if (ciphertext == NULL)
        {
            perror("malloc");
            MPI_Finalize();
            return 1;
        }

        MPI_Recv(ciphertext, cipher_len, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        unsigned char decrypted[512];
        unsigned long long decrypted_len = 0;

        if (chacha_decrypt(key,
                           ciphertext,
                           cipher_len,
                           nonce,
                           decrypted,
                           &decrypted_len) != 0)
        {
            fprintf(stderr, "Error descifrando en el servidor\n");
            free(ciphertext);
            MPI_Finalize();
            return 1;
        }

        printf("Servidor recibió y descifró: %s\n", decrypted);
        free(ciphertext);
    }

    MPI_Finalize();
    return 0;
}
