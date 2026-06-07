/*
 * chacha_lib_test_mpi.c
 * Ejemplo de uso de la librería `libchacha` en un entorno MPI.
 *
 * Flujo:
 * - Rank 0: genera una clave aleatoria, cifra un mensaje con chacha_encrypt,
 *   y envía (clave, longitud, nonce, ciphertext) a Rank 1.
 * - Rank 1: recibe los datos y llama a chacha_decrypt para recuperar el texto claro.
 *
 * Este ejemplo es educativo: en un sistema real no se debe enviar la clave
 * sin protección. Aquí se hace para demostrar la API y el cifrado/descifrado.
 */

#include <mpi.h>
#include <sodium.h>
#include <stdio.h>
#include <string.h>
#include "chacha.h"

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* Inicializar libsodium (llamar antes de usar la API) */
    if (sodium_init() < 0)
    {
        printf("Error iniciando libsodium\n");
        MPI_Finalize();
        return 1;
    }

    /* Generamos una clave aleatoria para la sesión (32 bytes). En producción
     * la clave debe ser intercambiada de forma segura (p. ej. ECDH + KDF).
     */
    unsigned char key[CHACHA_KEYBYTES];
    randombytes_buf(key, CHACHA_KEYBYTES);

    if (rank == 0)
    {
        const char *mensaje = "Hola desde rank 0";
        unsigned long long mlen = strlen(mensaje) + 1; // incluir NUL

        unsigned char nonce[CHACHA_NONCEBYTES];
        unsigned char ciphertext[512];
        unsigned long long clen = 0;

        /* Cifrar usando la biblioteca */
        if (chacha_encrypt(key, (const unsigned char*)mensaje, mlen, nonce, ciphertext, &clen) != 0)
        {
            printf("Error cifrando\n");
            MPI_Finalize();
            return 1;
        }

        /* Envío de datos: clave (solo para demo), longitud, nonce, ciphertext */
        MPI_Send(key, CHACHA_KEYBYTES, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);
        MPI_Send(&clen, 1, MPI_UNSIGNED_LONG_LONG, 1, 0, MPI_COMM_WORLD);
        MPI_Send(nonce, CHACHA_NONCEBYTES, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);
        MPI_Send(ciphertext, clen, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);

        printf("Rank 0 envio mensaje cifrado (len=%llu)\n", clen);
    }
    else if (rank == 1)
    {
        /* Recepción de clave (demo), longitud, nonce y ciphertext */
        unsigned char key_recv[CHACHA_KEYBYTES];
        unsigned long long clen = 0;
        MPI_Recv(key_recv, CHACHA_KEYBYTES, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&clen, 1, MPI_UNSIGNED_LONG_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        unsigned char nonce[CHACHA_NONCEBYTES];
        MPI_Recv(nonce, CHACHA_NONCEBYTES, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        unsigned char ciphertext[512];
        MPI_Recv(ciphertext, clen, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        unsigned char decrypted[512];
        unsigned long long dlen = 0;

        /* Descifrar y verificar autenticidad */
        if (chacha_decrypt(key_recv, ciphertext, clen, nonce, decrypted, &dlen) != 0)
        {
            printf("Error descifrando\n");
            MPI_Finalize();
            return 1;
        }

        /* Mostrar texto claro si la verificación fue exitosa */
        printf("Rank 1 descifro: %s\n", decrypted);
    }

    MPI_Finalize();
    return 0;
}
