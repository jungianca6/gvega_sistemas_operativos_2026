#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "../darknet/include/darknet.h"
#include "detector.h"

#define CFG_FILE     "../darknet/cfg/yolov3.cfg"
#define WEIGHTS_FILE "../darknet/yolov3.weights"
#define NAMES_FILE   "../darknet/data/coco.names"
#define OUTPUT_DIR   "../result"

static int path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static int ensure_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return mkdir(path, 0755);
}

static int build_output_path(int rank, const char *input_path, char *out_path, size_t out_path_size)
{
    const char *base = strrchr(input_path, '/');
    base = base ? base + 1 : input_path;
    int written = snprintf(out_path,
                           out_path_size,
                           "%s/rank_%d_%s",
                           OUTPUT_DIR,
                           rank,
                           base);
    return (written < 0 || (size_t)written >= out_path_size) ? -1 : 0;
}

static image **load_alphabet_from(const char *base_dir)
{
    int i, j;
    const int nsize = 8;
    image **alphabets = calloc(nsize, sizeof(image *));
    if (!alphabets) return NULL;

    for (j = 0; j < nsize; ++j)
    {
        alphabets[j] = calloc(128, sizeof(image));
        if (!alphabets[j])
        {
            for (int k = 0; k < j; ++k)
            {
                for (i = 32; i < 127; ++i)
                {
                    free_image(alphabets[k][i]);
                }
                free(alphabets[k]);
            }
            free(alphabets);
            return NULL;
        }
        for (i = 32; i < 127; ++i)
        {
            char buff[256];
            snprintf(buff, sizeof(buff), "%s/labels/%d_%d.png", base_dir, i, j);
            alphabets[j][i] = load_image_color(buff, 0, 0);
        }
    }
    return alphabets;
}

static void free_alphabet(image **alphabet)
{
    if (!alphabet) return;
    for (int j = 0; j < 8; ++j)
    {
        if (!alphabet[j]) continue;
        for (int i = 32; i < 127; ++i)
        {
            free_image(alphabet[j][i]);
        }
        free(alphabet[j]);
    }
    free(alphabet);
}

network *global_net = NULL;
int init_detector(void)
{
    if (global_net != NULL)
    {
        return 0;
    }

    global_net =
        load_network(
            CFG_FILE,
            WEIGHTS_FILE,
            0);

    if (!global_net)
    {
        fprintf(stderr,
                "Error cargando la red YOLO desde %s y %s\n",
                CFG_FILE,
                WEIGHTS_FILE);

        return -1;
    }

    set_batch_network(global_net, 1);

    return 0;
}

void cleanup_detector(void)
{
    if (global_net)
    {
        free_network(global_net);
        global_net = NULL;
    }
}

#define NUM_CLASSES  80

#define THRESH       0.5f
#define HIER_THRESH  0.5f
#define NMS_THRESH   0.45f



int detectar_imagen(int rank, const char *ruta_imagen, int class_counts[NUM_CLASSES])
{
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    printf("Rank %d cwd = %s\n", rank, cwd);
    printf("Rank %d CFG = %s\n", rank, CFG_FILE);
    printf("Rank %d WEIGHTS = %s\n", rank, WEIGHTS_FILE);
    printf("Rank %d NAMES = %s\n", rank, NAMES_FILE);

    fflush(stdout);
    
    printf("Rank %d: Procesando imagen: %s\n", rank, ruta_imagen);
    printf("Rank %d: CFG: %s\n", rank, CFG_FILE);
    printf("Rank %d: WEIGHTS: %s\n", rank, WEIGHTS_FILE);
    printf("Rank %d: NAMES: %s\n", rank, NAMES_FILE);
    fflush(stdout);

    for (int i = 0; i < NUM_CLASSES; ++i) {
        class_counts[i] = 0;
    }

    if (!path_exists(CFG_FILE) || !path_exists(WEIGHTS_FILE) || !path_exists(NAMES_FILE))
    {
        fprintf(stderr,
                "Error: falta archivo de modelo Darknet.\n"
                "  CFG: %s\n"
                "  WEIGHTS: %s\n"
                "  NAMES: %s\n",
                CFG_FILE,
                WEIGHTS_FILE,
                NAMES_FILE);
        fprintf(stderr,
                "Compruebe que los pesos YOLO estén presentes en el repositorio o configure los archivos correctos.\n");
        return 0;
    }

    if (init_detector() != 0)
    {
        return 0;
    }

    network *net = global_net;

    int net_w = network_width(net);
    int net_h = network_height(net);

    /* Cargar imagen */
    image im =
        load_image_color(
            (char *)ruta_imagen,
            0,
            0);
    if (!im.data)
    {
        fprintf(stderr, "No se pudo cargar la imagen: %s\n", ruta_imagen);
        return 0;
    }

    image resized =
        letterbox_image(
            im,
            net_w,
            net_h);

    /* Inferencia */
    network_predict(
        net,
        resized.data);

    int nboxes = 0;

    detection *dets =
        get_network_boxes(
            net,
            im.w,
            im.h,
            THRESH,
            HIER_THRESH,
            0,
            1,
            &nboxes);

    do_nms_sort(
        dets,
        nboxes,
        NUM_CLASSES,
        NMS_THRESH);

    char **names = get_labels((char *)NAMES_FILE);
    image **alphabet = load_alphabet_from("../darknet/data");

    if (!alphabet)
    {
        fprintf(stderr, "Error cargando el alfabeto de etiquetas desde ../darknet/data/labels\n");
    }
    else
    {
        /* Dibujar cajas y etiquetas sobre la imagen original */
        draw_detections(im, dets, nboxes, THRESH, names, alphabet, NUM_CLASSES);
    }

    int total_detectados = 0;

    for (int i = 0; i < nboxes; i++)
    {
        int best_class = -1;
        float best_prob = THRESH;

        for (int c = 0; c < NUM_CLASSES; c++)
        {
            if (dets[i].prob[c] > best_prob)
            {
                best_prob = dets[i].prob[c];
                best_class = c;
            }
        }

        if (best_class >= 0)
        {
            class_counts[best_class]++;
            total_detectados++;
        }
    }

    if (ensure_directory(OUTPUT_DIR) != 0)
    {
        fprintf(stderr, "Error creando directorio %s\n", OUTPUT_DIR);
    }
    else
    {
        char output_path[512];
        if (build_output_path(rank, ruta_imagen, output_path, sizeof(output_path)) == 0)
        {
            save_image(im, output_path);
            printf("Rank %d: Imagen anotada guardada en: %s\n",
                   rank,
                   output_path);
        }
        else
        {
            fprintf(stderr, "Error creando ruta de salida para imagen anotada\n");
        }
    }

    printf("Rank %d: Objetos detectados: %d\n",
           rank,
           total_detectados);

    free_alphabet(alphabet);

    free_detections(
        dets,
        nboxes);

    free_image(im);
    free_image(resized);

    return total_detectados;
}