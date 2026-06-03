/*
 * detector.c — Object Detection Summary using Darknet YOLOv3
 *
 * Scans the ../images/ directory, runs YOLOv3 on each image,
 * and prints a frequency-sorted summary of all detected objects.
 *
 * Compile:  make
 * Run:      ./detector
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "darknet.h"

/* ── Configuration ──────────────────────────────────────────────── */
#define IMAGES_DIR   "images"
#define CFG_FILE     "darknet/cfg/yolov3.cfg"
#define WEIGHTS_FILE "darknet/yolov3.weights"
#define NAMES_FILE   "darknet/data/coco.names"
#define NUM_CLASSES  80
#define THRESH       0.5f
#define HIER_THRESH  0.5f
#define NMS_THRESH   0.45f

/* ── Helpers ────────────────────────────────────────────────────── */

/* Check if a filename ends with one of the supported image extensions */
static int is_image_file(const char *name)
{
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;

    return (strcasecmp(ext, ".jpg")  == 0 ||
            strcasecmp(ext, ".jpeg") == 0 ||
            strcasecmp(ext, ".png")  == 0 ||
            strcasecmp(ext, ".bmp")  == 0);
}

/* Comparison function for qsort: sort by count descending */
typedef struct {
    int   class_id;
    int   count;
} class_count;

static int cmp_desc(const void *a, const void *b)
{
    return ((const class_count *)b)->count - ((const class_count *)a)->count;
}

/* ── Main ───────────────────────────────────────────────────────── */

int main(void)
{
    /* ── 1. Load YOLOv3 network ──────────────────────────────── */
    printf("Loading YOLOv3 network...\n");
    network *net = load_network(CFG_FILE, WEIGHTS_FILE, 0);
    set_batch_network(net, 1);

    int net_w = network_width(net);
    int net_h = network_height(net);

    /* ── 2. Load class names ─────────────────────────────────── */
    char **names = get_labels(NAMES_FILE);

    /* ── 3. Per-class counters ───────────────────────────────── */
    int counts[NUM_CLASSES];
    memset(counts, 0, sizeof(counts));

    /* ── 4. Scan images directory ────────────────────────────── */
    DIR *dir = opendir(IMAGES_DIR);
    if (!dir) {
        fprintf(stderr, "Error: could not open directory '%s'\n", IMAGES_DIR);
        free_network(net);
        return 1;
    }

    int total_images = 0;
    struct dirent *entry;

    printf("\n");

    while ((entry = readdir(dir)) != NULL) {
        if (!is_image_file(entry->d_name))
            continue;

        /* Build full path */
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", IMAGES_DIR, entry->d_name);

        /* Load and pre-process */
        image im      = load_image_color(path, 0, 0);
        image resized = letterbox_image(im, net_w, net_h);

        /* Run inference */
        network_predict(net, resized.data);

        /* Get detections */
        int nboxes = 0;
        detection *dets = get_network_boxes(net, im.w, im.h,
                                            THRESH, HIER_THRESH,
                                            0, 1, &nboxes);
        do_nms_sort(dets, nboxes, NUM_CLASSES, NMS_THRESH);

        /* Count detections above threshold */
        int img_count = 0;
        for (int i = 0; i < nboxes; i++) {
            for (int c = 0; c < NUM_CLASSES; c++) {
                if (dets[i].prob[c] > THRESH) {
                    counts[c]++;
                    img_count++;
                }
            }
        }

        printf("Processing: %s\n", path);
        printf(" -> %d object(s) detected\n", img_count);

        /* Cleanup */
        free_detections(dets, nboxes);
        free_image(im);
        free_image(resized);

        total_images++;
    }
    closedir(dir);

    if (total_images == 0) {
        printf("No image files found in '%s'.\n", IMAGES_DIR);
        free_network(net);
        return 0;
    }

    /* ── 5. Build sorted array ───────────────────────────────── */
    class_count sorted[NUM_CLASSES];
    int detected_classes = 0;
    int total_objects     = 0;

    for (int i = 0; i < NUM_CLASSES; i++) {
        if (counts[i] > 0) {
            sorted[detected_classes].class_id = i;
            sorted[detected_classes].count    = counts[i];
            total_objects += counts[i];
            detected_classes++;
        }
    }

    qsort(sorted, detected_classes, sizeof(class_count), cmp_desc);

    /* ── 6. Print summary table ──────────────────────────────── */
    #define W 34  /* inner width of the box */

    printf("\n");

    /* Top border: ╔══...══╗ */
    printf("\u2554");
    for (int i = 0; i < W; i++) printf("\u2550");
    printf("\u2557\n");

    /* Title */
    printf("\u2551      DETECTION RESULTS           \u2551\n");

    /* Separator: ╠══...══╣ */
    printf("\u2560");
    for (int i = 0; i < W; i++) printf("\u2550");
    printf("\u2563\n");

    /* Per-class rows */
    for (int i = 0; i < detected_classes; i++) {
        printf("\u2551  %-20s : %5d     \u2551\n",
               names[sorted[i].class_id], sorted[i].count);
    }

    /* Separator: ╠══...══╣ */
    printf("\u2560");
    for (int i = 0; i < W; i++) printf("\u2550");
    printf("\u2563\n");

    /* Total */
    printf("\u2551  Total objects detected: %5d  \u2551\n", total_objects);

    /* Bottom border: ╚══...══╝ */
    printf("\u255a");
    for (int i = 0; i < W; i++) printf("\u2550");
    printf("\u255d\n");

    /* ── 7. Most frequent object ─────────────────────────────── */
    if (detected_classes > 0) {
        printf("Most frequent object: \"%s\" with %d instance(s).\n",
               names[sorted[0].class_id], sorted[0].count);
    }

    /* ── Cleanup ─────────────────────────────────────────────── */
    free_network(net);
    /* Note: names array is allocated by get_labels, but darknet
       does not expose a free for it — it's fine, we're exiting. */

    printf("\nDone. Processed %d image(s).\n", total_images);
    return 0;
}
