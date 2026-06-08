#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include "chacha.h"
#include "detector.h"

#define MAX_IMAGE_PATH 512

static int write_file(const char *path,
                      const unsigned char *data,
                      unsigned long long len)
{
    FILE *file = fopen(path, "wb");
    if (!file) return -1;

    if (fwrite(data, 1, (size_t)len, file) != (size_t)len)
    {
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int get_basename(const char *path, char *out, size_t out_size)
{
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    int written = snprintf(out, out_size, "%s", base);
    return (written < 0 || (size_t)written >= out_size) ? -1 : 0;
}

static int write_temp_image(int rank,
                            const char *original_path,
                            const unsigned char *data,
                            unsigned long long len,
                            char *out_path,
                            size_t out_path_size)
{
    char base[MAX_IMAGE_PATH];
    if (get_basename(original_path, base, sizeof(base)) != 0)
    {
        return -1;
    }

    int written = snprintf(out_path,
                           out_path_size,
                           "/tmp/mpi_cliente_rank_%d_%s",
                           rank,
                           base);
    if (written < 0 || (size_t)written >= out_path_size)
    {
        return -1;
    }

    return write_file(out_path, data, len);
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
                    "Uso: mpirun -np 1 ./servidor <imagen> : -np 1 ./cliente\n");
        }
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

    if (rank == 1)
    {
        int path_len = 0;
        MPI_Recv(&path_len,
                 1,
                 MPI_INT,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        if (path_len <= 0 || path_len > MAX_IMAGE_PATH)
        {
            fprintf(stderr, "Cliente: nombre de imagen invalido\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        char image_path[MAX_IMAGE_PATH];
        MPI_Recv(image_path,
                 path_len,
                 MPI_CHAR,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        unsigned long long cipher_len = 0;
        MPI_Recv(&cipher_len,
                 1,
                 MPI_UNSIGNED_LONG_LONG,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        unsigned char nonce[CHACHA_NONCEBYTES];
        MPI_Recv(nonce,
                 CHACHA_NONCEBYTES,
                 MPI_UNSIGNED_CHAR,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        unsigned char *ciphertext = malloc((size_t)cipher_len);
        if (!ciphertext)
        {
            fprintf(stderr, "Cliente: error de memoria al recibir datos\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        MPI_Recv(ciphertext,
                 cipher_len,
                 MPI_UNSIGNED_CHAR,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        unsigned char *plaintext = malloc((size_t)cipher_len);
        if (!plaintext)
        {
            fprintf(stderr, "Cliente: error de memoria al descifrar\n");
            free(ciphertext);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        unsigned long long plain_len = 0;
        if (chacha_decrypt(key,
                           ciphertext,
                           cipher_len,
                           nonce,
                           plaintext,
                           &plain_len) != 0)
        {
            fprintf(stderr, "Cliente: error descifrando datos\n");
            free(ciphertext);
            free(plaintext);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        char tmp_path[MAX_IMAGE_PATH];
        if (write_temp_image(rank,
                             image_path,
                             plaintext,
                             plain_len,
                             tmp_path,
                             sizeof(tmp_path)) != 0)
        {
            fprintf(stderr, "Cliente: error escribiendo imagen temporal\n");
            free(ciphertext);
            free(plaintext);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        printf("Cliente: imagen recibida %s -> %s\n", image_path, tmp_path);
        fflush(stdout);

        int class_counts[NUM_CLASSES] = {0};
        int detected = detectar_imagen(rank, tmp_path, class_counts);
        remove(tmp_path);

        MPI_Send(&detected,
                 1,
                 MPI_INT,
                 0,
                 0,
                 MPI_COMM_WORLD);
        MPI_Send(class_counts,
                 NUM_CLASSES,
                 MPI_INT,
                 0,
                 0,
                 MPI_COMM_WORLD);

        free(ciphertext);
        free(plaintext);
    }

    MPI_Finalize();
    return 0;
}
