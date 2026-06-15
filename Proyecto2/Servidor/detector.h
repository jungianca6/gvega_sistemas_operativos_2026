#ifndef DETECTOR_H
#define DETECTOR_H

#define NUM_CLASSES 80

/*
 * Carga los nombres de las clases desde un archivo de etiquetas.
 * Devuelve un arreglo de cadenas terminadas en NULL.
 */
char **get_labels(char *filename);

/*
 * Procesa una imagen usando YOLO.
 * Rellena `class_counts` con el número de instancias detectadas por clase.
 * Devuelve la cantidad total de objetos detectados.
 *
 * rank: número de MPI del worker que está procesando la imagen.
 */
int detectar_imagen(int rank, const char *ruta_imagen, int class_counts[NUM_CLASSES]);

int init_detector(void);

void cleanup_detector(void);

#endif