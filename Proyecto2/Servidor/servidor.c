/*
 * servidor.c
 * Servidor MPI que orquesta procesamiento distribuido de imágenes
 * Rol: Recibe imágenes cifradas del cliente, descifra, distribuye a workers,
 *      agrega resultados y determina objeto más frecuente globalmente.
 * Componentes:
 *   - Rank 1: Servidor raíz (coordinador)
 *   - Ranks 2-4: Workers (procesamiento paralelo con Darknet)
 */

#include <dirent.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sodium.h>
#include "chacha.h"
#include "detector.h"

/* MPI Configuration */
#define IMAGE_WORKERS 3                        /* Número de workers para procesamiento */
#define CLIENT_RANK 0                          /* Rank del cliente (procesa imágenes) */
#define SERVER_ROOT_RANK 1                     /* Rank del servidor coordinador */
#define WORKER_RANK_START 2                    /* Inicio de ranks de workers */
#define WORKER_RANK_END (WORKER_RANK_START + IMAGE_WORKERS - 1) /* Fin de workers */
#define EXPECTED_SIZE (IMAGE_WORKERS + 2)      /* Total de procesos esperados */

/* Path Configuration */
#define MAX_IMAGE_PATH 512
#define TEMP_DIR "/tmp"                       /* Directorio para imágenes temporales */
#define RESULT_DIR "../result/"               /* Almacena imágenes con anotaciones */


/* Escribe datos binarios a un archivo */
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

/* Extrae nombre de archivo de la ruta completa */
static const char *get_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* Crea directorio si no existe */
static int ensure_directory(const char *path)
{
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
    {
        return -1;
    }
    return 0;
}

/* Elimina todos los archivos en un directorio (limpieza de resultados anteriores) */
static int clean_directory(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char file_path[MAX_IMAGE_PATH];
        int written = snprintf(file_path,
                               sizeof(file_path),
                               "%s%s",
                               path,
                               entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(file_path))
            continue;

        if (remove(file_path) != 0)
        {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    return 0;
}

/* Crea archivo temporal con imagen descifrada para procesamiento local del worker */
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
    /* Inicializar MPI (cliente + servidor + 3 workers) */
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char hostname[256];

    gethostname(hostname, sizeof(hostname));

    printf(
        "[MPI] Rank %d ejecutandose en %s\n",
        rank,
        hostname);

    fflush(stdout);


    /* Validar que tenemos exactamente 5 procesos (1 cliente + 1 servidor + 3 workers) */
    if (size != EXPECTED_SIZE)
    {
        if (rank == SERVER_ROOT_RANK)
        {
            fprintf(stderr,
                    "Uso: mpirun -np 1 ./cliente : -np 4 ./servidor\n");
        }
        MPI_Finalize();
        return 1;
    }

    /* Inicializar libsodium para ChaCha20 */
    if (sodium_init() < 0)
    {
        if (rank == SERVER_ROOT_RANK)
        {
            fprintf(stderr, "Error inicializando libsodium\n");
        }
        MPI_Finalize();
        return 1;
    }

    /* Servidor root: crear y limpiar directorios de almacenamiento */
    if (rank == SERVER_ROOT_RANK)
    {
        if (ensure_directory(RESULT_DIR) != 0)
        {
            fprintf(stderr, "Servidor: no se pudo crear directorio %s\n", RESULT_DIR);
            MPI_Finalize();
            return 1;
        }
        /* Limpiar resultados anteriores antes de procesar nuevas imágenes */
        if (clean_directory(RESULT_DIR) != 0)
        {
            fprintf(stderr, "Servidor: no se pudieron eliminar resultados anteriores en %s\n", RESULT_DIR);
            MPI_Finalize();
            return 1;
        }
    }

    unsigned char key[CHACHA_KEYBYTES];

    /* ========== SERVIDOR ROOT (Rank 1) ========== */
    if (rank == SERVER_ROOT_RANK)
    {
        /* 1. Recibir clave ChaCha20 del cliente */
        MPI_Recv(key,
                 CHACHA_KEYBYTES,
                 MPI_UNSIGNED_CHAR,
                 CLIENT_RANK,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        /* 2. Distribuir clave a todos los workers para descifrado local */
        for (int worker = WORKER_RANK_START; worker <= WORKER_RANK_END; ++worker)
        {
            MPI_Send(key,
                     CHACHA_KEYBYTES,
                     MPI_UNSIGNED_CHAR,
                     worker,
                     0,
                     MPI_COMM_WORLD);
        }

        /* 3. Recibir n\u00famero total de im\u00e1genes a procesar */
        int num_images = 0;
        MPI_Recv(&num_images,
                 1,
                 MPI_INT,
                 CLIENT_RANK,
                 1,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        if (num_images < 0)
        {
            fprintf(stderr, "Servidor: numero de imagenes invalido\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        printf("Servidor: recibiendo %d imagen(es) del cliente\n", num_images);
        fflush(stdout);

        for (int index = 0; index < num_images; ++index)
        {
            int path_len = 0;
            MPI_Recv(&path_len,
                     1,
                     MPI_INT,
                     CLIENT_RANK,
                     2,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            if (path_len <= 0 || path_len > MAX_IMAGE_PATH)
            {
                fprintf(stderr, "Servidor: ruta de imagen invalida\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            char image_path[MAX_IMAGE_PATH];
            MPI_Recv(image_path,
                     path_len,
                     MPI_CHAR,
                     CLIENT_RANK,
                     3,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            unsigned long long cipher_len = 0;
            MPI_Recv(&cipher_len,
                     1,
                     MPI_UNSIGNED_LONG_LONG,
                     CLIENT_RANK,
                     4,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            unsigned char nonce[CHACHA_NONCEBYTES];
            MPI_Recv(nonce,
                     CHACHA_NONCEBYTES,
                     MPI_UNSIGNED_CHAR,
                     CLIENT_RANK,
                     5,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            unsigned char *ciphertext = malloc((size_t)cipher_len);
            if (!ciphertext)
            {
                fprintf(stderr, "Servidor: error de memoria al recibir datos\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            MPI_Recv(ciphertext,
                     cipher_len,
                     MPI_UNSIGNED_CHAR,
                     CLIENT_RANK,
                     6,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            /* Descifrar datos en memoria (sin guardar archivo cifrado) */
            unsigned char *plaintext = malloc((size_t)cipher_len);
            if (!plaintext)
            {
                fprintf(stderr, "Servidor: error de memoria al descifrar datos\n");
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
                fprintf(stderr, "Servidor: error descifrando datos\n");
                free(ciphertext);
                free(plaintext);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            printf(
                "[COORDINADOR] Rank %d recibio %s (%llu bytes)\n",
                rank,
                image_path,
                plain_len);
            fflush(stdout);

            int worker_rank = WORKER_RANK_START + (index % IMAGE_WORKERS);
            printf(
                "[COORDINADOR] Enviando %s a worker rank %d\n",
                image_path,
                worker_rank);

            fflush(stdout);
            MPI_Send(&path_len,
                     1,
                     MPI_INT,
                     worker_rank,
                     1,
                     MPI_COMM_WORLD);
            MPI_Send(image_path,
                     path_len,
                     MPI_CHAR,
                     worker_rank,
                     2,
                     MPI_COMM_WORLD);
            MPI_Send(&cipher_len,
                     1,
                     MPI_UNSIGNED_LONG_LONG,
                     worker_rank,
                     3,
                     MPI_COMM_WORLD);
            MPI_Send(nonce,
                     CHACHA_NONCEBYTES,
                     MPI_UNSIGNED_CHAR,
                     worker_rank,
                     4,
                     MPI_COMM_WORLD);
            MPI_Send(ciphertext,
                     cipher_len,
                     MPI_UNSIGNED_CHAR,
                     worker_rank,
                     5,
                     MPI_COMM_WORLD);

            free(ciphertext);
            free(plaintext);
        }

        int termination_signal = 0;
        for (int worker = WORKER_RANK_START; worker <= WORKER_RANK_END; ++worker)
        {
            MPI_Send(&termination_signal,
                     1,
                     MPI_INT,
                     worker,
                     1,
                     MPI_COMM_WORLD);
        }

        int total_objects = 0;
        int global_counts[NUM_CLASSES] = {0};

        for (int i = 0; i < num_images; ++i)
        {
            int detected = 0;
            int class_counts[NUM_CLASSES] = {0};



            MPI_Recv(&detected,
                     1,
                     MPI_INT,
                     MPI_ANY_SOURCE,
                     10,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
            
            printf(
                "[COORDINADOR] Resultado recibido: %d objetos\n",
                detected);
            fflush(stdout);
            MPI_Recv(class_counts,
                     NUM_CLASSES,
                     MPI_INT,
                     MPI_ANY_SOURCE,
                     11,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            for (int c = 0; c < NUM_CLASSES; ++c)
            {
                global_counts[c] += class_counts[c];
            }
            total_objects += detected;
        }

        int best_class = -1;
        int best_count = 0;
        for (int c = 0; c < NUM_CLASSES; ++c)
        {
            if (global_counts[c] > best_count)
            {
                best_count = global_counts[c];
                best_class = c;
            }
        }

        const char *best_name = "ninguno";
        if (best_class >= 0)
        {
            char **names = get_labels("../darknet/data/coco.names");
            if (names && names[best_class])
            {
                best_name = names[best_class];
            }
        }

        int best_name_len = (int)strlen(best_name);
        printf("\n");
        printf("=====================================\n");
        printf("OBJETO MAS FRECUENTE GLOBAL\n");
        printf("%s -> %d veces\n",
               best_name,
               best_count);
        printf("TOTAL OBJETOS -> %d\n",
               total_objects);
        printf("=====================================\n");
        printf("\n");

        fflush(stdout);

        MPI_Send(&best_count,
                 1,
                 MPI_INT,
                 CLIENT_RANK,
                 6,
                 MPI_COMM_WORLD);
        MPI_Send(&best_name_len,
                 1,
                 MPI_INT,
                 CLIENT_RANK,
                 7,
                 MPI_COMM_WORLD);
        MPI_Send(best_name,
                 best_name_len,
                 MPI_CHAR,
                 CLIENT_RANK,
                 8,
                 MPI_COMM_WORLD);
        MPI_Send(&total_objects,
                 1,
                 MPI_INT,
                 CLIENT_RANK,
                 9,
                 MPI_COMM_WORLD);
    }
    else if (rank >= WORKER_RANK_START && rank <= WORKER_RANK_END)
    {
        MPI_Recv(key,
                 CHACHA_KEYBYTES,
                 MPI_UNSIGNED_CHAR,
                 SERVER_ROOT_RANK,
                 0,
                 MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        while (1)
        {
            int path_len = 0;
            MPI_Recv(&path_len,
                     1,
                     MPI_INT,
                     SERVER_ROOT_RANK,
                     1,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            if (path_len == 0)
            {
                break;
            }

            if (path_len <= 0 || path_len > MAX_IMAGE_PATH)
            {
                fprintf(stderr, "Worker %d: ruta invalida\n", rank);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            char image_path[MAX_IMAGE_PATH];
            MPI_Recv(image_path,
                     path_len,
                     MPI_CHAR,
                     SERVER_ROOT_RANK,
                     2,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            unsigned long long cipher_len = 0;
            MPI_Recv(&cipher_len,
                     1,
                     MPI_UNSIGNED_LONG_LONG,
                     SERVER_ROOT_RANK,
                     3,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            unsigned char nonce[CHACHA_NONCEBYTES];
            MPI_Recv(nonce,
                     CHACHA_NONCEBYTES,
                     MPI_UNSIGNED_CHAR,
                     SERVER_ROOT_RANK,
                     4,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            unsigned char *ciphertext = malloc((size_t)cipher_len);
            if (!ciphertext)
            {
                fprintf(stderr, "Worker %d: error de memoria\n", rank);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            MPI_Recv(ciphertext,
                     cipher_len,
                     MPI_UNSIGNED_CHAR,
                     SERVER_ROOT_RANK,
                     5,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            unsigned char *plaintext = malloc((size_t)cipher_len);
            if (!plaintext)
            {
                fprintf(stderr, "Worker %d: error de memoria\n", rank);
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
                fprintf(stderr, "Worker %d: error descifrando datos\n", rank);
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
                fprintf(stderr, "Worker %d: error creando imagen temporal\n", rank);
                free(ciphertext);
                free(plaintext);
                MPI_Abort(MPI_COMM_WORLD, 1);
            }

            printf("[WORKER %d]: imagen recibida %s -> %s\n",
                   rank,
                   image_path,
                   tmp_path);
            fflush(stdout);

            int class_counts[NUM_CLASSES] = {0};
            int detected = detectar_imagen(rank, tmp_path, class_counts);
            printf(
                "[WORKER %d] Detecto %d objetos\n",
                rank,
                detected);

            fflush(stdout);
            if (detected < 0)
            {
                detected = 0;
            }
            remove(tmp_path);


            MPI_Send(&detected,
                     1,
                     MPI_INT,
                     SERVER_ROOT_RANK,
                     10,
                     MPI_COMM_WORLD);
            MPI_Send(class_counts,
                     NUM_CLASSES,
                     MPI_INT,
                     SERVER_ROOT_RANK,
                     11,
                     MPI_COMM_WORLD);

            free(ciphertext);
            free(plaintext);
        }
    }

    MPI_Finalize();
    return 0;
}
