#define _GNU_SOURCE         // for getline()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>
#include "trie.h"
#include "doc_struct.h"
#include "util.h"

int interface(Trie *trie, char **docs, int *docWc);

int doc_count;

int main(int argc, char *argv[]) {
    char *fifo0, *fifo1;
    if (argc < 2 || !isdigit(*argv[1])) {
        ///
        printf("Error in args\n");
        return 27;
    }
    asprintf(&fifo0, "%s/Worker%d_0", PIPEPATH, atoi(argv[1]));
    asprintf(&fifo1, "%s/Worker%d_1", PIPEPATH, atoi(argv[1]));
    int fd0 = open(fifo0, O_RDONLY);
    int fd1 = 5;//open(fifo1, O_WRONLY | O_NONBLOCK);
    if (fd0 < 0 || fd1 < 0) {
        perror("fifo open error");
        return 1;
    }

    char msgbuf[BUFSIZ];
    StringListNode *dirnames = NULL;
    StringListNode *last_dirname = NULL;
    while (read(fd0, msgbuf, BUFSIZ) > 0) {
        if (dirnames == NULL) {     // only for first dir
            dirnames = createStringListNode(msgbuf);
            if (dirnames == NULL) {
                fprintf(stderr, "Failed to allocate memory.\n");
                return NULL;
            }
            last_dirname = dirnames;
        } else {
            last_dirname->next = createStringListNode(msgbuf);
            if (last_dirname->next == NULL) {
                fprintf(stderr, "Failed to allocate memory.\n");
                return NULL;
            }
            last_dirname = last_dirname->next;
        }
    }
//    while (dirnames != NULL) {
//        printf("%s\n", dirnames->string);
//        dirnames = dirnames->next;
//    }

    DIR* FD;
    struct dirent* curr_dirent;
    doc_count = 0;
    StringListNode *curr_dirname = dirnames;
    char full_name[PATH_MAX];
    while (curr_dirname != NULL) {
        if ((FD = opendir(curr_dirname->string)) == NULL) {
            fprintf(stderr, "Error : Failed to open input directory - %s\n", strerror(errno));
            return 1;
        }
        while ((curr_dirent = readdir(FD))) {
            if ((strcmp(curr_dirent->d_name, ".") != 0) && (strcmp(curr_dirent->d_name, "..") != 0)) {
                doc_count++;
            }
        }
        curr_dirname = curr_dirname->next;
    }

    char *docnames[doc_count];
    int doclines[doc_count];
    char **docs[doc_count];

    FILE *fp;
    curr_dirname = dirnames;
    int curr_doc = 0;
    int lines_num;
    size_t bufsize = 128;      // sample size - getline will reallocate memory as needed
    char *buffer = NULL;
    while (curr_dirname != NULL) {
        if ((FD = opendir(curr_dirname->string)) == NULL) {
            fprintf(stderr, "Error : Failed to open input directory - %s\n", strerror(errno));
            return 1;
        }
        while ((curr_dirent = readdir(FD))) {
            if (!strcmp(curr_dirent->d_name, ".") || !strcmp(curr_dirent->d_name, "..")) {
                continue;
            }
            sprintf(full_name, "%s/%s", curr_dirname->string, curr_dirent->d_name);
            fp = fopen(full_name, "rw");
            if (fp == NULL) {
                fprintf(stderr, "Error : Failed to open entry file - %s\n", strerror(errno));
                return 1;
            }
            docnames[curr_doc] = malloc(strlen(full_name) + 1);
            strcpy(docnames[curr_doc], full_name);
            lines_num = 0;
            while (getline(&buffer, &bufsize, fp) != -1) {
                lines_num++;
            }
            doclines[curr_doc] = lines_num;
            docs[curr_doc] = malloc(lines_num * sizeof(char*));
            rewind(fp);     // start again from the beginning of docfile
            for (int curr_line = 0; curr_line < lines_num; curr_line++) {
                if (getline(&buffer, &bufsize, fp) == -1) {
                    fprintf(stderr, "Something unexpected happened.\n");
                    return -1;
                }
                docs[curr_doc][curr_line] = malloc(strlen(buffer) + 1);
                strcpy(docs[curr_doc][curr_line], buffer);
            }
            fclose(fp);
            curr_doc++;
        }
        curr_dirname = curr_dirname->next;
    }
    if (buffer != NULL) {
        free(buffer);
    }
    for (int i = 0; i < doc_count; i++) {
        printf("File: %s\n", docnames[i]);
        for (int j = 0; j < doclines[i]; j++) {
            printf(" %d %s\n", j, docs[i][j]);
        }
    }

//
//
//
//    DIR* FD;
//    struct dirent* curr_dirent;
//    //char *dir_name = "/home/ch0sen/Desktop/MyProjects/DI/jobExecutor/cmake-build-debug/test";
//    char *dir_name = NULL;
//    FILE *entry_file;
//    char *full_name;
//    size_t bufsize = 128;      // sample size - getline will reallocate memory as needed
//    char *buffer = NULL;
//
//    while (getline(&dir_name, &bufsize, stdin) != -1) {
//        strtok(dir_name, "\n");
//        if ((FD = opendir(dir_name)) == NULL) {
//            fprintf(stderr, "Error : Failed to open input directory - %s\n", strerror(errno));
//            return 1;
//        }
//        while ((curr_dirent = readdir(FD))) {
//            if (!strcmp(curr_dirent->d_name, ".") || !strcmp(curr_dirent->d_name, "..")) {
//                continue;
//            }
//            full_name = malloc(sizeof(dir_name) + sizeof(curr_dirent->d_name) + 2);
//            sprintf(full_name, "%s/%s", dir_name, curr_dirent->d_name);
//            entry_file = fopen(full_name, "rw");
//            if (entry_file == NULL) {
//                fprintf(stderr, "Error : Failed to open entry file - %s\n", strerror(errno));
//                return 1;
//            }
//            while (getline(&buffer, &bufsize, entry_file) != -1) {
//                printf("%s", buffer);
//            }
//            fclose(entry_file);
//            free(full_name);
//        }
//    }
//    if (buffer != NULL) {
//        free(buffer);
//    }
//    if (dir_name != NULL) {
//        free(dir_name);
//    }


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
