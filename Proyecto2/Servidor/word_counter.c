#include <ctype.h>
#include "word_counter.h"
#include <string.h>

int buscar_palabra(
    WordCount tabla[],
    int cantidad,
    const char *palabra)
{
    for(int i=0;i<cantidad;i++)
    {
        if(strcmp(tabla[i].word, palabra) == 0)
        {
            return i;
        }
    }

    return -1;
}


int contar_palabras(
    char palabras[][MAX_WORD],
    int cantidad_palabras,
    WordCount tabla[])
{
    int total_unicas = 0;

    for(int i=0;i<cantidad_palabras;i++)
    {
        int idx =
            buscar_palabra(
                tabla,
                total_unicas,
                palabras[i]);

        if(idx == -1)
        {
            strcpy(
                tabla[total_unicas].word,
                palabras[i]);

            tabla[total_unicas].count = 1;

            total_unicas++;
        }
        else
        {
            tabla[idx].count++;
        }
    }

    return total_unicas;
}

void fusionar_tablas(
    WordCount global[],
    int *global_count,
    WordCount local[],
    int local_count)
{
    for(int i=0;i<local_count;i++)
    {
        int idx =
            buscar_palabra(
                global,
                *global_count,
                local[i].word);

        if(idx == -1)
        {
            global[*global_count] =
                local[i];

            (*global_count)++;
        }
        else
        {
            global[idx].count +=
                local[i].count;
        }
    }
}


WordCount buscar_maximo(
    WordCount tabla[],
    int cantidad)
{
    int max = 0;

    for(int i=1;i<cantidad;i++)
    {
        if(tabla[i].count >
           tabla[max].count)
        {
            max = i;
        }
    }

    return tabla[max];
}

void normalizar_palabra(char *palabra)
{
    int len = strlen(palabra);

    // Convertir a minúsculas
    for(int i = 0; i < len; i++)
    {
        palabra[i] = (char)tolower((unsigned char)palabra[i]);
    }

    // Eliminar caracteres no alfanuméricos al inicio
    int inicio = 0;

    while(inicio < len &&
          !isalnum((unsigned char)palabra[inicio]))
    {
        inicio++;
    }

    // Eliminar caracteres no alfanuméricos al final
    int fin = len - 1;

    while(fin >= inicio &&
          !isalnum((unsigned char)palabra[fin]))
    {
        fin--;
    }

    if(inicio > fin)
    {
        palabra[0] = '\0';
        return;
    }

    int nueva_len = fin - inicio + 1;

    memmove(
        palabra,
        palabra + inicio,
        nueva_len);

    palabra[nueva_len] = '\0';
}