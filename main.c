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
                return 4;
            }
            last_dirname = dirnames;
        } else {
            last_dirname->next = createStringListNode(msgbuf);
            if (last_dirname->next == NULL) {
                fprintf(stderr, "Failed to allocate memory.\n");
                return 4;
            }
            last_dirname = last_dirname->next;
        }
    }

    // First count the number of documents:
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
    for (int i = 0; i < doc_count; i++) {
        printf("File %d: %s\n", i, docnames[i]);
        for (int j = 0; j < doclines[i]; j++) {
            printf(" %d %s\n", j, docs[i][j]);
        }
    }


    const char *cmds[6] = {
            "/search",
            "/maxcount",
            "/mincount",
            "/wc",
            "/help",
            "/exit"
    };



    char *command;
    while (1) {
        printf("\n");
        // Until "/exit" is given, read current line and attempt to execute it as a command
        printf("Type a command:\n");
        getline(&buffer, &bufsize, stdin);
        bufferptr = buffer;
        strtok(buffer, "\n");     // remove trailing newline character
        command = strtok(buffer, " ");
        if (!strcmp(command, cmds[0]) || !strcmp(command, "/s")) {          // search
//            command = strtok(NULL, " \t");
//            if (command == NULL) {
//                fprintf(stderr, "Invalid use of '/search': At least one query term is required.\n");
//                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
//                continue;
//            }
//            char *terms[10];
//            terms[0] = command;
//            for (int i = 1; i < 10; i++) {
//                terms[i] = NULL;
//            }
//            int term_count = 1;
//            command = strtok(NULL, " \t");
//            while (command != NULL && term_count < 10) {
//                terms[term_count] = command;
//                term_count++;
//                command = strtok(NULL, " \t");
//            }
//
//            /* For each term, we'll start by keeping a pointer to its first postingList (postingListPtr array)
//             * Then, we'll iterate through all the docs adding and checking these pointers for each one.
//             * If the postingListPtr[i] points to a list with id less than the current's doc_id, we point it to its next.
//             * If we surpass it, since the listNodes are in order of id, there is no postinglist for this doc in that term.
//             * Else, if a match is found, we calculate the score() for this doc and term.
//             * Each doc that contained a search term is then added to a pairing heap for later printing. */
//            HeapNode *heap = NULL;
//            PostingListNode *postingListPtr[term_count];
//            PostingList *tempPostingList;
//            for (int i = 0; i < term_count; i++) {
//                tempPostingList = getPostingList(trie, terms[i]);
//                postingListPtr[i] = (tempPostingList == NULL) ? NULL : tempPostingList->first;
//            }
//            double doc_score;
//            int tf;
//            char found;
//            for (int id = 0; id < doc_count; id++) {
//                doc_score = 0;
//                found = 0;
//                for (int i = 0; i < term_count; i++) {
//                    while (postingListPtr[i] != NULL && postingListPtr[i]->id < id) {
//                        postingListPtr[i] = postingListPtr[i]->next;
//                    }
//                    // We suprassed the id - term is not contained in current doc
//                    if (postingListPtr[i] == NULL || postingListPtr[i]->id > id) {
//                        continue;
//                    }
//                    // Else doc_id exists in this posting list:
//                    tempPostingList = getPostingList(trie, terms[i]);
//                    tf = getTermFrequency(tempPostingList, id);
//                    if (tf <= 0) {   // getPostingList() returned NULL <=> word doesn't exist in trie
//                        continue;
//                    }
//                    found = 1;
//                    //doc_score += score(tf, tempPostingList->df, docWc[id]);
//                }
//                if (found) {
//                    heap = heapInsert(heap, doc_score, id);
//                }
//            }
//            //int exit_code = print_results(&heap, docs, terms);
//            if (heap != NULL) {
//                destroyHeap(&heap);
//            }
//            if (exit_code > 0) {      // failed to print results (due to an inability to allocate memory)
//                return exit_code;
//            }
        } else if (!strcmp(command, cmds[1])) {       // maxcount
            char *keyword = strtok(NULL, " \t");
            PostingList *keywordPostingList = getPostingList(trie, keyword);
            if (keywordPostingList == NULL) {
                printf("'%s' doesn't exist in docs.\n", keyword);
                continue;
            }
            PostingListNode *current = keywordPostingList->first;
            int max_id = keywordPostingList->first->id;
            int max_tf = keywordPostingList->first->tf;
            current = current->next;
            while (current != NULL) {
                if (current->tf > max_tf || (current->tf == max_tf && (strcmp(docnames[current->id], docnames[max_id]) < 0))) {
                    max_id = current->id;
                    max_tf = current->tf;
                }
                current = current->next;
            }
            printf("'%s' appears the most in \"%s\". //(%d times)//\n", keyword, docnames[max_id], max_tf);
        } else if (!strcmp(command, cmds[2])) {       // mincount
            char *keyword = strtok(NULL, " \t");
            PostingList *keywordPostingList = getPostingList(trie, keyword);
            if (keywordPostingList == NULL) {
                printf("'%s' doesn't exist in docs.\n", keyword);
                continue;
            }
            PostingListNode *current = keywordPostingList->first;
            int min_id = keywordPostingList->first->id;
            int min_tf = keywordPostingList->first->tf;
            current = current->next;
            while (current != NULL) {
                if (current->tf < min_tf || (current->tf == min_tf && (strcmp(docnames[current->id], docnames[min_id]) < 0))) {
                    min_id = current->id;
                    min_tf = current->tf;
                }
                current = current->next;
            }
            printf("'%s' appears the least in \"%s\". //(%d times)//\n", keyword, docnames[min_id], min_tf);
        } else if (!strcmp(command, cmds[3])) {       // wc

        } else if (!strcmp(command, cmds[4])) {             /// not here
            printf("Available commands (use without quotes):\n");
            printf(" '/search word1 word2 ... -d sec' for a list of the files that include the given words, along with the lines where they appear. Results will be printed within the seconds given as a deadline.\n");
            printf(" '/maxcount word' for the file where the given word appears the most.\n");
            printf(" '/mincount word' for the file where the given word appears the least (but at least once).\n");
            printf(" '/wc' for the number of characters (bytes), words and lines of every file.\n");
            printf(" '/help' for the list you're seeing right now.\n");
            printf(" '/exit' to terminate this program.\n");
        } else if (!strcmp(command, cmds[5])) {       // exit
            break;
        } else {
            fprintf(stderr, "Unknown command '%s': Type '/help' for a detailed list of available commands.\n", command);
        }
        buffer = bufferptr;
    }



    if (bufferptr != NULL) {
        free(bufferptr);
    }
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
