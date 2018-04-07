#define _GNU_SOURCE         // for getline()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "trie.h"
#include "pairingheap.h"

extern int doc_count;

int interface(Trie *trie, char **docs, int *docWc) {

    const char *cmds[6] = {
            "/search",
            "/maxcount",
            "/mincount",
            "/wc",
            "/help",
            "/exit"
    };

    char *command;
    size_t bufsize = 32;   ///   // sample size - getline will reallocate memory as needed
    char *buffer = malloc(bufsize);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 4;
    }
    char *bufferptr;       // used to free buffer after using strtok
    while (1) {
        printf("\n");
        // Until "/exit" is given, read current line and attempt to execute it as a command
        printf("Type a command:\n");
        getline(&buffer, &bufsize, stdin);
        bufferptr = buffer;
        strtok(buffer, "\n");     // remove trailing newline character
        command = strtok(buffer, " ");
        if (!strcmp(command, cmds[0]) || !strcmp(command, "/s")) {          // search
            command = strtok(NULL, " \t");
            if (command == NULL) {
                fprintf(stderr, "Invalid use of '/search': At least one query term is required.\n");
                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
                continue;
            }
            char *terms[10];
            terms[0] = command;
            for (int i = 1; i < 10; i++) {
                terms[i] = NULL;
            }
            int term_count = 1;
            command = strtok(NULL, " \t");
            while (command != NULL && term_count < 10) {
                terms[term_count] = command;
                term_count++;
                command = strtok(NULL, " \t");
            }

            /* For each term, we'll start by keeping a pointer to its first postingList (postingListPtr array)
             * Then, we'll iterate through all the docs adding and checking these pointers for each one.
             * If the postingListPtr[i] points to a list with id less than the current's doc_id, we point it to its next.
             * If we surpass it, since the listNodes are in order of id, there is no postinglist for this doc in that term.
             * Else, if a match is found, we calculate the score() for this doc and term.
             * Each doc that contained a search term is then added to a pairing heap for later printing. */
            HeapNode *heap = NULL;
            PostingListNode *postingListPtr[term_count];
            PostingList *tempPostingList;
            for (int i = 0; i < term_count; i++) {
                tempPostingList = getPostingList(trie, terms[i]);
                postingListPtr[i] = (tempPostingList == NULL) ? NULL : tempPostingList->first;
            }
            double doc_score;
            int tf;
            char found;
            for (int id = 0; id < doc_count; id++) {
                doc_score = 0;
                found = 0;
                for (int i = 0; i < term_count; i++) {
                    while (postingListPtr[i] != NULL && postingListPtr[i]->id < id) {
                        postingListPtr[i] = postingListPtr[i]->next;
                    }
                    // We suprassed the id - term is not contained in current doc
                    if (postingListPtr[i] == NULL || postingListPtr[i]->id > id) {
                        continue;
                    }
                    // Else doc_id exists in this posting list:
                    tempPostingList = getPostingList(trie, terms[i]);
                    tf = getTermFrequency(tempPostingList, id);
                    if (tf <= 0) {   // getPostingList() returned NULL <=> word doesn't exist in trie
                        continue;
                    }
                    found = 1;
                    //doc_score += score(tf, tempPostingList->df, docWc[id]);
                }
                if (found) {
                    heap = heapInsert(heap, doc_score, id);
                }
            }
            //int exit_code = print_results(&heap, docs, terms);
            if (heap != NULL) {
                destroyHeap(&heap);
            }
//            if (exit_code > 0) {      // failed to print results (due to an inability to allocate memory)
//                return exit_code;
//            }
        } else if (!strcmp(command, cmds[1])) {       // maxcount

        } else if (!strcmp(command, cmds[2])) {       // mincount

        } else if (!strcmp(command, cmds[3])) {       // wc

        } else if (!strcmp(command, cmds[4])) {       // help
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
    free(bufferptr);
    return 0;
}

