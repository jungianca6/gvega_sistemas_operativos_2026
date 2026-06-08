/*
 * cliente.c
 * Cliente MPI que escanea imágenes, las cifra con ChaCha20 y las envía al servidor.
 * Rol: Automatizar carga de imágenes desde ../images, cifrar datos binarios,
 *      y transmitirlos al servidor root vía MPI para procesamiento distribuido.
 * Componentes:
 *   - Rank 0: Cliente (escaneo, cifrado, envío)
 */

#include <mpi.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include "chacha.h"

/* MPI Configuration */
#define IMAGE_WORKERS 3                        /* Número de workers en el servidor */
#define CLIENT_RANK 0                          /* Este proceso (rango del cliente) */
#define SERVER_ROOT_RANK 1                     /* Rank del servidor coordinador */
#define EXPECTED_SIZE (IMAGE_WORKERS + 2)      /* Total de procesos esperados */

/* Path Configuration */
#define IMAGE_DIR "../images"                 /* Directorio de imágenes a procesar */
#define MAX_IMAGE_PATH 1024

/* Verifica si el archivo tiene extensión de imagen reconocida (.jpg, .jpeg, .png, .bmp) */
static int is_image_file(const char *name)
{
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;

    char lower_ext[16];
    size_t idx = 0;
    for (const char *p = ext; *p && idx + 1 < sizeof(lower_ext); ++p)
    {
        lower_ext[idx++] = (char)tolower((unsigned char)*p);
    }
    lower_ext[idx] = '\0';

    return strcmp(lower_ext, ".jpg") == 0 ||
           strcmp(lower_ext, ".jpeg") == 0 ||
           strcmp(lower_ext, ".png") == 0 ||
           strcmp(lower_ext, ".bmp") == 0;
}

/*
 * Escanea un directorio y recopila todas las rutas de imágenes.
 * out_paths: array dinámico de strings (rutas)
 * out_count: número de imágenes encontradas
 */
static int collect_images(const char *directory,
                          char ***out_paths,
                          int *out_count)
{
    DIR *dp = opendir(directory);
    if (!dp) return -1;

    char **paths = NULL;
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.') continue;  /* Saltar . y .. */
        if (!is_image_file(entry->d_name)) continue;  /* Solo archivos de imagen */

        char full_path[MAX_IMAGE_PATH];
        int written = snprintf(full_path,
                               sizeof(full_path),
                               "%s/%s",
                               directory,
                               entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(full_path)) continue;

        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;  /* Validar que es archivo regular */

        char *image_path = strdup(full_path);
        if (!image_path)
        {
            closedir(dp);
            for (int i = 0; i < count; ++i) free(paths[i]);
            free(paths);
            return -1;
        }

        char **tmp = realloc(paths, sizeof(char *) * (count + 1));
        if (!tmp)
        {
            free(image_path);
            closedir(dp);
            for (int i = 0; i < count; ++i) free(paths[i]);
            free(paths);
            return -1;
        }

        paths = tmp;
        paths[count++] = image_path;
    }

    closedir(dp);
    *out_paths = paths;
    *out_count = count;
    return 0;
}

/* Lee archivo completo en memoria */
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
    /* Inicializar MPI */
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Solo el cliente (rank 0) ejecuta este código */
    if (rank != CLIENT_RANK)
    {
        MPI_Finalize();
        return 0;
    }

    /* Validar que tenemos exactamente 5 procesos (1 cliente + 1 servidor + 3 workers) */
    if (size != EXPECTED_SIZE)
    {
        fprintf(stderr,
                "Cliente: este cliente debe ejecutarse con %d procesos totales (1 cliente + 1 servidor + %d workers).\n",
                EXPECTED_SIZE,
                IMAGE_WORKERS);
        MPI_Finalize();
        return 1;
    }

    /* Determinar directorio de imágenes: argumento o default */
    const char *image_dir = IMAGE_DIR;
    if (argc == 2)
    {
        image_dir = argv[1];
    }

    char **image_paths = NULL;
    int num_images = 0;
    if (collect_images(image_dir, &image_paths, &num_images) != 0 || num_images == 0)
    {
        fprintf(stderr, "Cliente: no se encontraron imágenes en %s\n", image_dir);
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
    randombytes_buf(key, CHACHA_KEYBYTES);
    MPI_Send(key,
             CHACHA_KEYBYTES,
             MPI_UNSIGNED_CHAR,
             SERVER_ROOT_RANK,
             0,
             MPI_COMM_WORLD);

    MPI_Send(&num_images,
             1,
             MPI_INT,
             SERVER_ROOT_RANK,
             1,
             MPI_COMM_WORLD);

    for (int i = 0; i < num_images; ++i)
    {
        const char *image_path = image_paths[i];
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
                 SERVER_ROOT_RANK,
                 2,
                 MPI_COMM_WORLD);
        MPI_Send(image_path,
                 path_len,
                 MPI_CHAR,
                 SERVER_ROOT_RANK,
                 3,
                 MPI_COMM_WORLD);
        MPI_Send(&cipher_len,
                 1,
                 MPI_UNSIGNED_LONG_LONG,
                 SERVER_ROOT_RANK,
                 4,
                 MPI_COMM_WORLD);
        MPI_Send(nonce,
                 CHACHA_NONCEBYTES,
                 MPI_UNSIGNED_CHAR,
                 SERVER_ROOT_RANK,
                 5,
                 MPI_COMM_WORLD);
        MPI_Send(ciphertext,
                 cipher_len,
                 MPI_UNSIGNED_CHAR,
                 SERVER_ROOT_RANK,
                 6,
                 MPI_COMM_WORLD);

        printf("Cliente: imagen enviada %s al servidor\n", image_path);
        fflush(stdout);

        free(image_data);
        free(ciphertext);
    }

    for (int i = 0; i < num_images; ++i)
    {
        free(image_paths[i]);
    }
    free(image_paths);

    int top_count = 0;
    int top_name_len = 0;
    MPI_Recv(&top_count,
             1,
             MPI_INT,
             SERVER_ROOT_RANK,
             6,
             MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
    MPI_Recv(&top_name_len,
             1,
             MPI_INT,
             SERVER_ROOT_RANK,
             7,
             MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

    char *top_name = malloc((size_t)top_name_len + 1);
    if (!top_name)
    {
        fprintf(stderr, "Cliente: error de memoria al recibir resultado\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Recv(top_name,
             top_name_len,
             MPI_CHAR,
             SERVER_ROOT_RANK,
             8,
             MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
    top_name[top_name_len] = '\0';

    int total_objects = 0;
    MPI_Recv(&total_objects,
             1,
             MPI_INT,
             SERVER_ROOT_RANK,
             9,
             MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

    printf("Cliente: resultado recibido: %s repetido %d veces, total objetos %d\n",
           top_name,
           top_count,
           total_objects);

    free(top_name);
    MPI_Finalize();
    return 0;
}
