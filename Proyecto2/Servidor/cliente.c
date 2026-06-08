#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include "chacha.h"
#include "detector.h"

#define MAX_IMAGE_PATH 512

static int read_file(const char *path,
                     unsigned char **out,
                     unsigned long long *len)
{
    FILE *file = fopen(path, "rb");
    if (!file) return -1;

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return -1;
    }

    long size = ftell(file);
    if (size < 0)
    {
        fclose(file);
        return -1;
    }

    rewind(file);

    unsigned char *buffer = malloc((size_t)size);
    if (!buffer)
    {
        fclose(file);
        return -1;
    }

    if (fread(buffer, 1, (size_t)size, file) != (size_t)size)
    {
        free(buffer);
        fclose(file);
        return -1;
    }

    fclose(file);
    *out = buffer;
    *len = (unsigned long long)size;
    return 0;
}

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
            fprintf(stderr,
                    "Uso: mpirun -np 1 ./servidor : -np 1 ./cliente <imagen>\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (rank != 1)
    {
        MPI_Finalize();
        return 0;
    }

    if (argc != 2)
    {
        fprintf(stderr, "Cliente: debe especificar la ruta de una imagen\n");
        MPI_Finalize();
        return 1;
    }

    if (sodium_init() < 0)
    {
        fprintf(stderr, "Cliente: error inicializando libsodium\n");
        MPI_Finalize();
        return 1;
    }

    unsigned char key[CHACHA_KEYBYTES];
    MPI_Bcast(key, CHACHA_KEYBYTES, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    const char *image_path = argv[1];
    unsigned char *image_data = NULL;
    unsigned long long image_len = 0;

    if (read_file(image_path, &image_data, &image_len) != 0)
    {
        fprintf(stderr, "Cliente: no se pudo leer la imagen %s\n", image_path);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    unsigned char nonce[CHACHA_NONCEBYTES];
    unsigned char *ciphertext = malloc(image_len + crypto_aead_xchacha20poly1305_ietf_ABYTES);
    if (!ciphertext)
    {
        fprintf(stderr, "Cliente: error de memoria al cifrar la imagen\n");
        free(image_data);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    unsigned long long cipher_len = 0;
    if (chacha_encrypt(key,
                      image_data,
                      image_len,
                      nonce,
                      ciphertext,
                      &cipher_len) != 0)
    {
        fprintf(stderr, "Cliente: error cifrando %s\n", image_path);
        free(image_data);
        free(ciphertext);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int path_len = (int)strlen(image_path) + 1;
    MPI_Send(&path_len,
             1,
             MPI_INT,
             0,
             0,
             MPI_COMM_WORLD);
    MPI_Send(image_path,
             path_len,
             MPI_CHAR,
             0,
             0,
             MPI_COMM_WORLD);
    MPI_Send(&cipher_len,
             1,
             MPI_UNSIGNED_LONG_LONG,
             0,
             0,
             MPI_COMM_WORLD);
    MPI_Send(nonce,
             CHACHA_NONCEBYTES,
             MPI_UNSIGNED_CHAR,
             0,
             0,
             MPI_COMM_WORLD);
    MPI_Send(ciphertext,
             cipher_len,
             MPI_UNSIGNED_CHAR,
             0,
             0,
             MPI_COMM_WORLD);

    printf("Cliente: imagen enviada %s\n", image_path);
    fflush(stdout);

    int detected = 0;
    int class_counts[NUM_CLASSES] = {0};

    MPI_Recv(&detected,
             1,
             MPI_INT,
             0,
             0,
             MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
    MPI_Recv(class_counts,
             NUM_CLASSES,
             MPI_INT,
             0,
             0,
             MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

    printf("Cliente: resultado recibido: %d objetos\n", detected);

    free(image_data);
    free(ciphertext);

    MPI_Finalize();
    return 0;
}
