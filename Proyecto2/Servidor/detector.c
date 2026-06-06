#include <stdio.h>
#include <string.h>

#include "../darknet/include/darknet.h"
#include "detector.h"

#define CFG_FILE     "../darknet/cfg/yolov3.cfg"
#define WEIGHTS_FILE "../darknet/yolov3.weights"
#define NAMES_FILE   "../darknet/data/coco.names"

#define NUM_CLASSES  80

#define THRESH       0.5f
#define HIER_THRESH  0.5f
#define NMS_THRESH   0.45f

int detectar_imagen(const char *ruta_imagen)
{
    printf("Procesando imagen: %s\n", ruta_imagen);

    printf("CFG: %s\n", CFG_FILE);
    printf("WEIGHTS: %s\n", WEIGHTS_FILE);
    printf("NAMES: %s\n", NAMES_FILE);
    fflush(stdout);

    /* Cargar red YOLO */
    network *net =
        load_network(
            CFG_FILE,
            WEIGHTS_FILE,
            0);

    set_batch_network(net, 1);

    int net_w = network_width(net);
    int net_h = network_height(net);



    /* Cargar imagen */
    image im =
        load_image_color(
            (char *)ruta_imagen,
            0,
            0);

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

    int total_detectados = 0;

    for(int i = 0; i < nboxes; i++)
    {
        for(int c = 0; c < NUM_CLASSES; c++)
        {
            if(dets[i].prob[c] > THRESH)
            {
                total_detectados++;
            }
        }
    }

    printf("Objetos detectados: %d\n",
           total_detectados);

    free_detections(
        dets,
        nboxes);

    free_image(im);
    free_image(resized);

    free_network(net);

    return total_detectados;
}