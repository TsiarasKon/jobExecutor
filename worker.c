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
#include "paths.h"
#include "util.h"

int interface(Trie *trie, char **docs, int *docWc);

int worker(int w_id) {
    int exit_code = 0;
    char fifo0[PATH_MAX], fifo1[PATH_MAX];
    sprintf(fifo0, "%s/Worker%d_0", PIPEPATH, w_id);
    sprintf(fifo1, "%s/Worker%d_1", PIPEPATH, w_id);
    int fd0 = open(fifo0, O_RDONLY | O_NONBLOCK);
    int fd1 = 5;//open(fifo1, O_WRONLY | O_NONBLOCK);
    if (fd0 < 0 || fd1 < 0) {
        perror("Error opening pipes");
        return EC_FIFO;
    }
int dir_count = 0;

    char msgbuf[BUFSIZ];
    StringListNode *dirnames = NULL;
    StringListNode *last_dirname = NULL;
    while (read(fd0, msgbuf, BUFSIZ) > 0) {
        if (dirnames == NULL) {     // only for first dir
            if ((dirnames = createStringListNode(msgbuf)) == NULL) {
                return EC_MEM;
            }
            last_dirname = dirnames;
        } else {
            if ((last_dirname->next = createStringListNode(msgbuf)) == NULL) {
                return EC_MEM;
            }
            last_dirname = last_dirname->next;
        }

        dir_count++;
    }
    printf("Worker%d: Got %d dirs\n", w_id, dir_count);

    // First count the number of documents:
    DIR *FD;
    struct dirent *curr_dirent;
    int doc_count = 0;
    StringListNode *curr_dirname = dirnames;
    while (curr_dirname != NULL) {
        if ((FD = opendir(curr_dirname->string)) == NULL) {
            perror("Error opening directory");
            return EC_DIR;
        }
        while ((curr_dirent = readdir(FD))) {
            if ((strcmp(curr_dirent->d_name, ".") != 0) && (strcmp(curr_dirent->d_name, "..") != 0)) {
                doc_count++;
            }
        }
        curr_dirname = curr_dirname->next;
    }
    printf("Worker%d: Got %d files\n", w_id, doc_count);

    char *docnames[doc_count];
    int doclines[doc_count];
    char **docs[doc_count];
    Trie *trie = createTrie();
    if (trie == NULL) {
        return EC_MEM;
    }
    FILE *fp;
    curr_dirname = dirnames;
    int curr_doc = 0;
    int lines_num;
    char *word;
    char symb_name[PATH_MAX + 1], full_name[PATH_MAX + 1];
    size_t bufsize = 128;      // sample size - getline will reallocate memory as needed
    char *buffer = NULL, *bufferptr = NULL;
    while (curr_dirname != NULL) {
        if ((FD = opendir(curr_dirname->string)) == NULL) {
            perror("Error opening directory");
            return EC_DIR;
        }
        while ((curr_dirent = readdir(FD))) {
            if (!strcmp(curr_dirent->d_name, ".") || !strcmp(curr_dirent->d_name, "..")) {
                continue;
            }
            sprintf(symb_name, "%s/%s", curr_dirname->string, curr_dirent->d_name);
            if (realpath(symb_name, full_name) == NULL) {       // getting full file path
                perror("Invalid file name");
                return EC_FILE;
            }
            fp = fopen(full_name, "r");
            if (fp == NULL) {
                perror("Failed to open file");
                return EC_FILE;
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
                perror("malloc");
                return EC_MEM;
            }
            rewind(fp);     // start again from the beginning of docfile
            for (int curr_line = 0; curr_line < lines_num; curr_line++) {
                if (getline(&buffer, &bufsize, fp) == -1) {
                    perror("Error");
                    return EC_UNKNOWN;
                }
                bufferptr = buffer;
                strtok(buffer, "\n");
                docs[curr_doc][curr_line] = malloc(strlen(buffer) + 1);
                if (docs[curr_doc][curr_line] == NULL) {
                    perror("malloc");
                    return EC_MEM;
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

//    for (int i = 0; i < doc_count; i++) {
//        printf("File %d: %s\n", i, docnames[i]);
//        for (int j = 0; j < doclines[i]; j++) {
//            printf(" %d %s\n", j, docs[i][j]);
//        }
//    }


    const char *cmds[6] = {
            "/search",
            "/maxcount",
            "/mincount",
            "/wc",
            "/help",
            "/exit"
    };

    char *logfile;
    asprintf(&logfile, "%s/Worker%d", LOGPATH, getpid());
    FILE *logfp = fopen(logfile, "w");
    char *command;
    while (1) {
        printf("\n");
        // Until "/exit" is given, read current line and attempt to execute it as a command
        printf("Type a command:\n");
        getline(&buffer, &bufsize, stdin);
        bufferptr = buffer;
        strtok(buffer, "\n");     // remove trailing newline character
        command = strtok(buffer, " \t");
        if (!strcmp(command, cmds[0])) {          // search
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL || !strcmp(keyword, "-d")) {
                fprintf(stderr, "Invalid use of '/search': At least one query term is required.\n");
                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
                continue;
            }
            StringListNode *first_term = createStringListNode(keyword);
            StringListNode *last_term = first_term;
            keyword = strtok(NULL, " \t");
            while (keyword != NULL && (strcmp(keyword, "-d") != 0)) {
                last_term->next = createStringListNode(keyword);
                last_term = last_term->next;
                keyword = strtok(NULL, " \t");
            }
            if (keyword == NULL || (strcmp(keyword, "-d") != 0)) {
                fprintf(stderr, "Invalid use of '/search': No deadline specified.\n");
                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
                continue;
            }
            keyword = strtok(NULL, " \t");
            if (keyword == NULL || !isdigit(*keyword)) {
                fprintf(stderr, "Invalid use of '/search': Invalid deadline.\n");
                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
                continue;
            }
            int deadline = atoi(keyword);       ///
            StringListNode *currTerm = first_term;
            while (currTerm != NULL) {
                PostingList *keywordPostingList = getPostingList(trie, currTerm->string);
                if (keywordPostingList == NULL) {     // current term doesn't exist in trie
                    fprintf(logfp, "%s : %s : %s :\n", getCurrentTime(), cmds[0] + 1, currTerm->string);
                    currTerm = currTerm->next;
                    continue;
                }
                fprintf(logfp, "%s : %s : %s", getCurrentTime(), cmds[0] + 1, currTerm->string);
                PostingListNode *currNode = keywordPostingList->first;
                while (currNode != NULL) {
                    fprintf(logfp, " : %s", docnames[currNode->id]);
                    IntListNode *currLine = currNode->firstline;
                    while (currLine != NULL) {
                        printf("%s %d: %s\n", docnames[currNode->id], currLine->line, docs[currNode->id][currLine->line]);
                        currLine = currLine->next;
                    }
                    currNode = currNode->next;
                }
                fprintf(logfp, "\n");
                currTerm = currTerm->next;
            }
        } else if (!strcmp(command, cmds[1])) {       // maxcount
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL) {
                fprintf(stderr, "Invalid use of '/maxcount' - Type '/help' to see the correct syntax.\n");
                continue;
            }
            PostingList *keywordPostingList = getPostingList(trie, keyword);
            if (keywordPostingList == NULL) {
                printf("'%s' doesn't exist in docs.\n", keyword);
                fprintf(logfp, "%s : %s : %s :\n", getCurrentTime(), cmds[1] + 1, keyword);
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
            printf("'%s' appears the most in \"%s\".\n", keyword, docnames[max_id]);
            fprintf(logfp, "%s : %s : %s : %s\n", getCurrentTime(), cmds[1] + 1, keyword, docnames[max_id]);
        } else if (!strcmp(command, cmds[2])) {       // mincount
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL) {
                fprintf(stderr, "Invalid use of '/mincount' - Type '/help' to see the correct syntax.\n");
                continue;
            }
            PostingList *keywordPostingList = getPostingList(trie, keyword);
            if (keywordPostingList == NULL) {
                printf("'%s' doesn't exist in docs.\n", keyword);
                fprintf(logfp, "%s : %s : %s :\n", getCurrentTime(), cmds[2] + 1, keyword);
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
            printf("'%s' appears the least in \"%s\".\n", keyword, docnames[min_id]);
            fprintf(logfp, "%s : %s : %s : %s\n", getCurrentTime(), cmds[2] + 1, keyword, docnames[min_id]);
        } else if (!strcmp(command, cmds[3])) {       // wc
            int total_chars = 0, total_words = 0, total_lines = 0;
            FILE *pp;
            char command_wc[PATH_MAX + 6];
            for (curr_doc = 0; curr_doc < doc_count; curr_doc++) {
                sprintf(command_wc, "wc \"%s\"", docnames[curr_doc]);
                pp = popen(command_wc, "r");
                if (pp == NULL) {
                    perror("Failed to run command");
                    return EC_CMD;
                }
                buffer = bufferptr;
                if (getline(&buffer, &bufsize, pp) == -1) {
                    perror("Failed to run command");
                    return EC_CMD;
                }
                //printf("%s\n", buffer);
                total_chars += atoi(strtok(buffer, " \t"));
                total_words += atoi(strtok(NULL, " \t"));
                total_lines += atoi(strtok(NULL, " \t"));
                pclose(pp);
            }
            printf("Worker bytes: %d\n", total_chars);
            printf("Worker words: %d\n", total_words);
            printf("Worker lines: %d\n", total_lines);
            fprintf(logfp, "%s : %s\n", getCurrentTime(), cmds[3] + 1);
        }        /// not here
        else if (!strcmp(command, cmds[4])) {
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
            /// ?
            fprintf(stderr, "Unknown command '%s': Type '/help' for a detailed list of available commands.\n", command);
        }
        buffer = bufferptr;
    }


    fclose(logfp);
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
    printf("Worker%d has exited.\n", getpid());
    return exit_code;
}