#ifndef WORD_COUNTER_H
#define WORD_COUNTER_H

#define MAX_WORD 64
#define MAX_WORDS 1024

typedef struct
{
    char word[MAX_WORD];
    int count;
} WordCount;

int buscar_palabra(
    WordCount tabla[],
    int cantidad,
    const char *palabra);

int contar_palabras(
    char palabras[][MAX_WORD],
    int cantidad_palabras,
    WordCount tabla[]);

void fusionar_tablas(
    WordCount global[],
    int *global_count,
    WordCount local[],
    int local_count);

WordCount buscar_maximo(
    WordCount tabla[],
    int cantidad);

void normalizar_palabra(
    char *palabra);

#endif