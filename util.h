#ifndef UTIL_H
#define UTIL_H

#include "trie.h"
#include "pairingheap.h"

double IDF(int df);
double score(int tf, int df, int D);

char word_in(char *word, char **word_list);
int print_results(HeapNode **heap, char **docs, char **terms);

#endif
