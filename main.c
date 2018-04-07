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
    int exit_code = 0;
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

    DIR *FD;
    struct dirent *curr_dirent;
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
    Trie *trie = createTrie();
    if (trie == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 4;
    }
    FILE *fp;
    curr_dirname = dirnames;
    int curr_doc = 0;
    int lines_num;
    char *word;
    size_t bufsize = 128;      // sample size - getline will reallocate memory as needed
    char *buffer = NULL, *bufferptr;
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
            docs[curr_doc] = malloc(lines_num * sizeof(char *));
            if (docs[curr_doc] == NULL) {
                fprintf(stderr, "Failed to allocate memory.\n");
                return 4;
            }
            rewind(fp);     // start again from the beginning of docfile
            for (int curr_line = 0; curr_line < lines_num; curr_line++) {
                if (getline(&buffer, &bufsize, fp) == -1) {
                    fprintf(stderr, "Something unexpected happened.\n");
                    return -1;
                }
                bufferptr = buffer;
                strtok(buffer, "\n");
                docs[curr_doc][curr_line] = malloc(strlen(buffer) + 1);
                if (docs[curr_doc][curr_line] == NULL) {
                    fprintf(stderr, "Failed to allocate memory.\n");
                    return 4;
                }
                strcpy(docs[curr_doc][curr_line], buffer);
                // insert line's words to trie
                word = strtok(buffer, " \t");     // get first word
                while (word != NULL) {          // for every word in doc
                    exit_code = insert(trie, word, curr_doc, curr_line);
                    if (exit_code > 0) {
                        return exit_code;
                    }
                    word = strtok(NULL, " \t");
                }
                buffer = bufferptr;
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
        printf("File %d: %s\n", i, docnames[i]);
        for (int j = 0; j < doclines[i]; j++) {
            printf(" %d %s\n", j, docs[i][j]);
        }
    }
    printf("%d %d\n", getTermFrequency(getPostingList(trie, "never"), 5), getTermFrequency(getPostingList(trie, "sdgfs"), 3));

    deleteTrie(&trie);
    for (int i = 0; i < doc_count; i++) {
        for (int j = 0; j < doclines[i]; j++) {
            free(docs[i][j]);
        }
        free(docs[i]);
    }
    return exit_code;
}

/* Exit codes:
 * 0: Success
 * 1: Invalid command line arguments
 * 2: Failed to open given file
 * 3: Docs in file not in order
 * 4: Memory allocation failed
 * -1: Some unexpected error
*/
