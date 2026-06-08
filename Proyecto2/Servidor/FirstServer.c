#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include "chacha.h"
#include "detector.h"

#define IMAGE_WORKERS 3
#define MAX_IMAGE_PATH 512
#define TEMP_DIR "/tmp"

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

static const char *get_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int write_temp_image(int rank,
                            const char *original_path,
                            const unsigned char *data,
                            unsigned long long len,
                            char *out_path,
                            size_t out_path_size)
{
    const char *base = get_basename(original_path);
    int written = snprintf(out_path,
                           out_path_size,
                           "%s/mpi_rank_%d_%s",
                           TEMP_DIR,
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

    if (size != IMAGE_WORKERS + 1)
    {
        if (rank == 0)
        {
            fprintf(stderr,
                    "Uso: mpirun -np 4 ./Servidor imagen1 imagen2 imagen3\n");
        }

        MPI_Finalize();
        return 1;
    }

    if (rank == 0 && argc != IMAGE_WORKERS + 1)
    {
        fprintf(stderr,
                "Debe especificar %d rutas de imagen\n",
                IMAGE_WORKERS);
        MPI_Finalize();
        return 1;
    }

    if (sodium_init() < 0)
    {
        if (rank == 0)
        {
            fprintf(stderr, "Error inicializando libsodium\n");
        }
        MPI_Finalize();
        return 1;
    }

    unsigned char key[CHACHA_KEYBYTES];
    randombytes_buf(key, CHACHA_KEYBYTES);
    MPI_Bcast(key, CHACHA_KEYBYTES, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        for (int destino = 1; destino <= IMAGE_WORKERS; destino++)
        {
            const char *image_path = argv[destino];
            unsigned char *image_data = NULL;
            unsigned long long image_len = 0;

            if (read_file(image_path, &image_data, &image_len) != 0)
            {
                fprintf(stderr,
                        "No se pudo leer la imagen: %s\n",
                        image_path);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            unsigned char nonce[CHACHA_NONCEBYTES];
            unsigned char *ciphertext = malloc(image_len + crypto_aead_xchacha20poly1305_ietf_ABYTES);
            unsigned long long cipher_len = 0;

            if (!ciphertext)
            {
                fprintf(stderr, "Error de memoria al cifrar imagen\n");
                free(image_data);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            if (chacha_encrypt(key,
                              image_data,
                              image_len,
                              nonce,
                              ciphertext,
                              &cipher_len) != 0)
            {
                fprintf(stderr, "Error cifrando %s\n", image_path);
                free(image_data);
                free(ciphertext);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            int path_len = (int)strlen(image_path) + 1;
            MPI_Send(&path_len, 1, MPI_INT, destino, 0, MPI_COMM_WORLD);
            MPI_Send(image_path, path_len, MPI_CHAR, destino, 0, MPI_COMM_WORLD);
            MPI_Send(&cipher_len, 1, MPI_UNSIGNED_LONG_LONG, destino, 0, MPI_COMM_WORLD);
            MPI_Send(nonce, CHACHA_NONCEBYTES, MPI_UNSIGNED_CHAR, destino, 0, MPI_COMM_WORLD);
            MPI_Send(ciphertext, cipher_len, MPI_UNSIGNED_CHAR, destino, 0, MPI_COMM_WORLD);

            printf("Rank 0 envio imagen cifrada %s a rank %d\n",
                   image_path,
                   destino);

            free(image_data);
            free(ciphertext);
        }

        int total_objects = 0;

        for (int origen = 1; origen <= IMAGE_WORKERS; origen++)
        {
            int detected = 0;
            MPI_Recv(&detected,
                     1,
                     MPI_INT,
                     origen,
                     0,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            printf("Rank %d detecto %d objetos\n", origen, detected);
            total_objects += detected;
        }

        printf("Total de objetos detectados: %d\n", total_objects);
    }
    else
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
            fprintf(stderr, "Nombre de imagen invalido en rank %d\n", rank);
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

        unsigned char *ciphertext = malloc(cipher_len);
        if (!ciphertext)
        {
            fprintf(stderr, "Error de memoria en rank %d\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        MPI_Recv(ciphertext,
                 cipher_len,
                 MPI_UNSIGNED_CHAR,
                 0,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        unsigned char *plaintext = malloc(cipher_len);
        if (!plaintext)
        {
            fprintf(stderr, "Error de memoria en rank %d\n", rank);
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
            fprintf(stderr, "Error descifrando datos en rank %d\n", rank);
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
            fprintf(stderr, "No se pudo escribir imagen temporal en rank %d\n", rank);
            free(ciphertext);
            free(plaintext);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        free(ciphertext);
        free(plaintext);

        int detected = detectar_imagen(tmp_path);
        if (detected < 0)
        {
            detected = 0;
        }
        remove(tmp_path);

        MPI_Send(&detected,
                 1,
                 MPI_INT,
                 0,
                 0,
                 MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}

