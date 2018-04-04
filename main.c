#define _GNU_SOURCE         // for getline()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include "trie.h"

int interface(Trie *trie, char **docs, int *docWc);

int doc_count;

int main(int argc, char *argv[]) {
    DIR* FD;
    struct dirent* in_file;
    char *dir = "/home/ch0sen/Desktop/MyProjects/DI/jobExecutor/cmake-build-debug/test";
    FILE *entry_file;
    char buffer[256];
    char full_name[512];

    if ((FD = opendir (dir)) == NULL) {
        fprintf(stderr, "Error : Failed to open input directory - %s\n", strerror(errno));

        return 1;
    }
    while ((in_file = readdir(FD))) {
        if (!strcmp (in_file->d_name, ".") || !strcmp (in_file->d_name, "..")) {
            continue;
        }
        sprintf(full_name, "%s/%s", dir, in_file->d_name);
        entry_file = fopen(full_name, "rw");
        if (entry_file == NULL) {
            fprintf(stderr, "Error : Failed to open entry file - %s\n", strerror(errno));

            return 1;
        }
        while (fgets(buffer, 256, entry_file) != NULL) {
            printf("%s", buffer);
        }
        fclose(entry_file);
    }

    return 0;
    ///

//    FILE *fp = fopen(docfile, "r");
//    if (fp == NULL) {
//        fprintf(stderr, "Couldn't open '%s'.\n", docfile);
//        return 2;
//    }
//    printf("Loading docs from '%s'...\n", docfile);
//    size_t bufsize = 128;      // sample size - getline will reallocate memory as needed
//    char *buffer = NULL, *bufferptr = NULL;
//    for (int i = 0; ; i++) {
//        if (getline(&buffer, &bufsize, fp) == -1) {
//            break;
//        }
//        bufferptr = buffer;
//        if (buffer[0] == '\n' || (buffer[0] == '\r' && buffer[1] == '\n'))  {     // ignore empty lines
//            continue;
//        }
//        while (*buffer == ' ' || *buffer == '\t') {     // ignore whitespace before id
//            buffer++;
//        }
//        if (atoi(buffer) != i) {
//            fprintf(stderr, "Error in '%s' - Docs not in order.\n", docfile);
//            return 3;
//        }
//        doc_count++;
//        buffer = bufferptr;     // resetting buffer that maybe was moved
//    }
//    rewind(fp);     // start again from the beginning of docfile
//
//    Trie* trie = createTrie();
//    if (trie == NULL) {
//        fprintf(stderr, "Failed to allocate memory.\n");
//        return 4;
//    }
//    char *docs[doc_count];      // "map"
//    char *word;
//    int docWc[doc_count];
//    int exit_code;
//    for (int id = 0; id < doc_count; id++) {
//        docWc[id] = 0;
//        if (getline(&buffer, &bufsize, fp) == -1) {
//            fprintf(stderr, "Something unexpected happened.\n");
//            return -1;
//        }
//        bufferptr = buffer;
//        while (*buffer == ' ' || *buffer == '\t') {     // ignore whitespace before id
//            buffer++;
//        }
//        while (isdigit(*buffer)) {          // ignore the id itself
//            buffer++;
//        }
//        strtok(buffer, "\r\n");         // remove trailing newline character
//        docs[id] = malloc(strlen(buffer) + 1);
//        if (docs[id] == NULL) {
//            fprintf(stderr, "Failed to allocate memory.\n");
//            return 4;
//        }
//        strcpy(docs[id], buffer);
//        word = strtok(buffer, " \t");     // get first word
//        while (word != NULL) {          // for every word in doc
//            exit_code = insert(trie, word, id);
//            if (exit_code > 0) {
//                return exit_code;
//            }
//            docWc[id]++;
//            avgdl++;
//            word = strtok(NULL, " \t");
//        }
//        buffer = bufferptr;     // resetting buffer that was moved
//    }
//    if (bufferptr != NULL) {
//        free(bufferptr);
//    }
//    fclose(fp);
//    printf("Docs loaded successfully!\n");
//
//    exit_code = interface(trie, docs, docWc);
//
//    deleteTrie(&trie);
//    for (int i = 0; i < doc_count; i++) {
//        free(docs[i]);
//    }
//    return exit_code;
}

/* Exit codes:
 * 0: Success
 * 1: Invalid command line arguments
 * 2: Failed to open given file
 * 3: Docs in file not in order
 * 4: Memory allocation failed
 * -1: Some unexpected error
*/
