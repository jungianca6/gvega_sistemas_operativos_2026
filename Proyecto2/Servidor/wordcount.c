#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_WORDS 1024
#define MAX_TEXT_LENGTH 20000

typedef struct {
    char *word;
    int count;
} Word;

typedef struct {
    Word words[MAX_WORDS];
} WordList;

int word_in_index( WordList *word_list, char *word, int wc ) {
    for( int i = 0; i < wc; i++ ) {
        if( strcmp( word_list->words[i].word, word ) == 0 ) {
            return i;
        }
    }

    return -1;
}

void sort_word_list( WordList *word_list, int wc ) {
    for( int i = 0; i < wc; i++ ) {
        int flipped = 0;
        for( int j = 0; j < wc - i - 1; j++ ) {
            if( word_list->words[j].count < word_list->words[j+1].count ) {
                Word temp = word_list->words[j];
                word_list->words[j] = word_list->words[j+1];
                word_list->words[j+1] = temp;
                flipped = 1;
            }
        }
        if( flipped == 0 ) {
            break;
        }
    }
}

int main( int argc, char *argv[] ) {
    if( argc < 2 ) {
        printf( "Usage: %s INPUT_FILE\n", argv[0] );
        return 1;
    }

    FILE *file = fopen( argv[1], "r" );
    char text[MAX_TEXT_LENGTH];
    fread( text, sizeof( char ), MAX_TEXT_LENGTH, file );
    fclose( file );

    char *word = strtok( text, " " );

    WordList *word_list = malloc( sizeof( WordList ) );

    int wc = 0;
    while( word != NULL ) {
        int index = word_in_index( word_list, word, wc );
        if( index == -1 ) {
            word_list->words[wc].word = word;
            word_list->words[wc].count = 1;
            wc++;
        } else {
            word_list->words[index].count++;
        }
        word = strtok( NULL, " " );
    }

    sort_word_list( word_list, wc );

    for( int i = 0; i < wc; i++ ) {
        char *s = ( word_list->words[i].count == 1 ? "" : "s" );
        printf( "'%s' occurs %d time%s\n", word_list->words[i].word, word_list->words[i].count, s );
    }

    return 0;
}