#include <mpi.h>
#include <sodium.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Inicializar libsodium
    if (sodium_init() < 0)
    {
        printf("Error iniciando libsodium\n");
        MPI_Finalize();
        return 1;
    }

    // Clave compartida de 32 bytes
    unsigned char key[crypto_stream_chacha20_ietf_KEYBYTES] =
        "12345678901234567890123456789012";

    if(rank == 0)
    {
        char mensaje[] = "Hola desde rank 0";

        unsigned char nonce[crypto_stream_chacha20_ietf_NONCEBYTES];

        randombytes_buf(nonce, sizeof nonce);

        unsigned long long len = strlen(mensaje) + 1;

        unsigned char ciphertext[256];

        // CIFRAR
        crypto_stream_chacha20_ietf_xor(
            ciphertext,
            (unsigned char*)mensaje,
            len,
            nonce,
            key
        );

        // Enviar tamaño
        MPI_Send(&len,
                 1,
                 MPI_UNSIGNED_LONG_LONG,
                 1,
                 0,
                 MPI_COMM_WORLD);

        // Enviar nonce
        MPI_Send(nonce,
                 sizeof nonce,
                 MPI_UNSIGNED_CHAR,
                 1,
                 0,
                 MPI_COMM_WORLD);

        // Enviar mensaje cifrado
        MPI_Send(ciphertext,
                 len,
                 MPI_UNSIGNED_CHAR,
                 1,
                 0,
                 MPI_COMM_WORLD);

        printf("Rank 0 envio mensaje cifrado\n");
    }

    else if(rank == 1)
    {
        unsigned long long len;

        // Recibir tamaño
        MPI_Recv(&len,
                 1,
                 MPI_UNSIGNED_LONG_LONG,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        unsigned char nonce[crypto_stream_chacha20_ietf_NONCEBYTES];

        // Recibir nonce
        MPI_Recv(nonce,
                 sizeof nonce,
                 MPI_UNSIGNED_CHAR,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        unsigned char ciphertext[256];

        // Recibir mensaje cifrado
        MPI_Recv(ciphertext,
                 len,
                 MPI_UNSIGNED_CHAR,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        unsigned char decrypted[256];

        // DESCIFRAR
        crypto_stream_chacha20_ietf_xor(
            decrypted,
            ciphertext,
            len,
            nonce,
            key
        );

        printf("Rank 1 descifro: %s\n", decrypted);
    }

    MPI_Finalize();
    return 0;
}