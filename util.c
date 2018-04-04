#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <math.h>
#include "util.h"

#define k1 1.2
#define b 0.75

extern int K;
extern int doc_count;
extern double avgdl;

double IDF(int df) {
    return log10((doc_count - df + 0.5) / (df + 0.5));
}

double score(int tf, int df, int D) {
    return IDF(df) * ((tf * (k1 + 1)) / (tf + k1 * (1 - b + b * (D / avgdl))));
}

/* Returns 1 or 0 if a string exists or dosen't exist respectively in a given
 * list of strings of any length, as long as the string after the last is NULL */
char word_in(char *word, char **word_list) {
    int i = 0;
    while (word_list[i] != NULL) {
        if (strcmp(word, word_list[i]) == 0) {
            return 1;
        }
        i++;
    }
    return 0;
}

int print_results(HeapNode **heap, char **docs, char **terms) {
    if (*heap == NULL) {
        printf("No results found.\n");
        return 0;
    }
    int margins[5];
    int heapSize = getHeapSize(*heap);
    if (K < heapSize) {     // the smallest of the two equals the number of search results
        margins[1] = (K == 1) ? 1 : (((int) log10(K - 1)) + 1);
    } else {
        margins[1] = (heapSize == 1) ? 1 : (((int) log10(heapSize - 1)) + 1);
    }
    margins[2] = (doc_count == 1) ? 1 : (((int) log10(doc_count - 1)) + 1);
    margins[3] = 4;         // (-) + up to 2 integer digits + decimal point
    margins[4] = 4;         // score decimal precision
    margins[0] = margins[1] + margins[2] + margins[3] + margins[4] + 6;    // total margin sum including parenthesis, braces, etc
    struct winsize w;
    for (int k = 0; k < K; k++) {
        if (*heap == NULL) {
            break;
        }
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        if (w.ws_col <= margins[0] + 1) {       // terminal too narrow - cannot pretty print
            fprintf(stderr, "Terminal too narrow - Results will be printed unaligned.\n");
            printf("%*d.(%*d)[%*.*f] %s\n", margins[1], k, margins[2], (*heap)->id, margins[3] + margins[4], margins[4], (*heap)->score, docs[(*heap)->id]);
            *heap = deleteMaxNode(heap);
        } else {
            int curr_col;
            size_t curr_word_len;
            int cols_to_write = w.ws_col - margins[0];
            char underlines[cols_to_write];
            char result = 0;    // 0 for the first time, 1 for the rest, -1 for '^'
            char *curr_doc = malloc(strlen(docs[(*heap)->id]) + 1);
            if (curr_doc == NULL) {
                fprintf(stderr, "Failed to allocate memory.\n");
                return 4;
            }
            char *curr_doc_ptr = curr_doc;      // used to free curr_doc after using strtok()
            strcpy(curr_doc, docs[(*heap)->id]);
            char *curr_word = strtok(curr_doc, " \t");
            while (curr_word != NULL || result <= 0) {
                if (result >= 0) {
                    if (result == 0) {      // first line to be printed
                        printf("%*d.(%*d)[%*.*f] ", margins[1], k, margins[2], (*heap)->id, margins[3] + margins[4], margins[4], (*heap)->score);
                        result = 1;
                    } else {
                        for (int i = 0; i < margins[0]; i++) {
                            printf(" ");
                        }
                    }
                    for (int i = 0; i < cols_to_write; i++) {
                        underlines[i] = ' ';
                    }
                    curr_col = 0;
                    while (curr_col < cols_to_write - 1 && curr_word != NULL) {
                        curr_word_len = strlen(curr_word);
                        // Word will never fit in terminal - abort printing:
                        if (curr_word_len > cols_to_write - 1) {
                            fprintf(stderr, "Terminal too narrow - Results printing was aborted.\n");
                            return -2;
                        }
                        // Word doesn't currently fit in terminal - we'll print it in the next line
                        if (curr_word_len > cols_to_write - curr_col - 1) {
                            break;
                        }
                        // Word is a search term - set corresponding underlines to '^'
                        if (word_in(curr_word, terms) == 1) {
                            for (int i = curr_col; i < curr_col + curr_word_len; i++) {
                                underlines[i] = '^';
                            }
                            result = -1;    // next loop we'll print the underlines (only if a term was found in this line)
                        }
                        for (int i = 0; i < curr_word_len; i++) {
                            printf("%c", curr_word[i]);
                            curr_col++;
                        }
                        printf(" ");
                        curr_col++;
                        curr_word = strtok(NULL, " \t");
                    }
                    printf("\n");
                } else {
                    for (int i = 0; i < margins[0]; i++) {
                        printf(" ");
                    }
                    for (curr_col = 0; curr_col < cols_to_write; curr_col++) {
                        printf("%c", underlines[curr_col]);
                    }
                    result = 1;
                }
            }
            free(curr_doc_ptr);
            *heap = deleteMaxNode(heap);
        }
    }
    return 0;
}
