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

static int get_basename(const char *path, char *out, size_t out_size)
{
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    int written = snprintf(out, out_size, "%s", base);
    return (written < 0 || (size_t)written >= out_size) ? -1 : 0;
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
                           "/tmp/mpi_rank_%d_%s",
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

    if (rank == 0 && argc != 2)
    {
        fprintf(stderr,
                "Debe especificar la ruta de una sola imagen.\n");
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
    if (rank == 0)
    {
        randombytes_buf(key, CHACHA_KEYBYTES);
    }

    MPI_Bcast(key, CHACHA_KEYBYTES, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        const char *image_path = argv[1];
        unsigned char *image_data = NULL;
        unsigned long long image_len = 0;

        if (read_file(image_path, &image_data, &image_len) != 0)
        {
            fprintf(stderr, "No se pudo leer la imagen: %s\n", image_path);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        unsigned char nonce[CHACHA_NONCEBYTES];
        unsigned char *ciphertext = malloc(image_len + crypto_aead_xchacha20poly1305_ietf_ABYTES);
        if (!ciphertext)
        {
            fprintf(stderr, "Error de memoria al cifrar imagen\n");
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
            fprintf(stderr, "Error cifrando %s\n", image_path);
            free(image_data);
            free(ciphertext);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        int path_len = (int)strlen(image_path) + 1;
        MPI_Send(&path_len, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        MPI_Send(image_path, path_len, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        MPI_Send(&cipher_len, 1, MPI_UNSIGNED_LONG_LONG, 1, 0, MPI_COMM_WORLD);
        MPI_Send(nonce, CHACHA_NONCEBYTES, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);
        MPI_Send(ciphertext, cipher_len, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);

        printf("Servidor: imagen cifrada enviada a cliente: %s\n", image_path);
        fflush(stdout);

        free(image_data);
        free(ciphertext);

        int detected = 0;
        int class_counts[NUM_CLASSES] = {0};
        MPI_Recv(&detected, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(class_counts, NUM_CLASSES, MPI_INT, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("Servidor: resultado recibido del cliente\n");
        printf("  Objetos detectados: %d\n", detected);

        if (detected > 0)
        {
            char **names = get_labels("../darknet/data/coco.names");
            int overall_best = -1;
            int best_count = 0;
            int first = 1;
            printf("  Clases detectadas:");
            for (int c = 0; c < NUM_CLASSES; c++)
            {
                if (class_counts[c] > 0)
                {
                    const char *label = (names && names[c]) ? names[c] : "clase";
                    if (!first) printf(",");
                    printf(" %s(%d)", label, class_counts[c]);
                    first = 0;
                    if (class_counts[c] > best_count)
                    {
                        best_count = class_counts[c];
                        overall_best = c;
                    }
                }
            }
            if (first)
            {
                printf(" ninguna");
            }
            printf("\n");

            if (overall_best >= 0)
            {
                const char *overall_name = (names && names[overall_best]) ? names[overall_best] : "clase";
                printf("  Mayor frecuencia: %s (%d)\n", overall_name, best_count);
            }
        }
    }
    else
    {
        int path_len = 0;
        MPI_Recv(&path_len, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (path_len <= 0 || path_len > MAX_IMAGE_PATH)
        {
            fprintf(stderr, "Cliente: nombre de imagen invalido\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        char image_path[MAX_IMAGE_PATH];
        MPI_Recv(image_path, path_len, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        unsigned long long cipher_len = 0;
        MPI_Recv(&cipher_len, 1, MPI_UNSIGNED_LONG_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        unsigned char nonce[CHACHA_NONCEBYTES];
        MPI_Recv(nonce, CHACHA_NONCEBYTES, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        unsigned char *ciphertext = malloc((size_t)cipher_len);
        if (!ciphertext)
        {
            fprintf(stderr, "Cliente: error de memoria al recibir datos\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        MPI_Recv(ciphertext, cipher_len, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

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
        if (write_temp_image(rank, image_path, plaintext, plain_len, tmp_path, sizeof(tmp_path)) != 0)
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

        MPI_Send(&detected, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send(class_counts, NUM_CLASSES, MPI_INT, 0, 0, MPI_COMM_WORLD);

        free(ciphertext);
        free(plaintext);
    }

    MPI_Finalize();
    return 0;
}
