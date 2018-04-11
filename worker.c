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
#include <signal.h>
#include "trie.h"
#include "paths.h"
#include "util.h"


/// TODO handle SIGINT

int worker(int w_id) {
    int pid = getpid();
    int exit_code = EC_OK;
    char fifo0[PATH_MAX], fifo1[PATH_MAX];
    sprintf(fifo0, "%s/Worker%d_0", PIPEPATH, w_id);
    sprintf(fifo1, "%s/Worker%d_1", PIPEPATH, w_id);
    int fd1 = open(fifo1, O_WRONLY | O_NONBLOCK);
    int fd0 = open(fifo0, O_RDONLY);
    if (fd0 < 0 || fd1 < 0) {
        perror("Error opening pipes");
        return EC_PIPE;
    }

    char msgbuf[BUFSIZ];
    StringList *dirnames = createStringList();
    if (dirnames == NULL) {
        return EC_MEM;
    }
    while (read(fd0, msgbuf, BUFSIZ) > 0) {
        if (*msgbuf == '$') {
            break;
        }
        if (appendStringListNode(dirnames, msgbuf) != EC_OK) {
            return EC_MEM;
        }
    }

    // First count the number of documents:
    DIR *FD;
    struct dirent *curr_dirent;
    int doc_count = 0;
    StringListNode *curr_dirname = dirnames->first;
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
//    printf("Worker #%d docs: %d\n", w_id, doc_count);

    char *docnames[doc_count];
    int doclines[doc_count];
    char **docs[doc_count];
    Trie *trie = createTrie();
    if (trie == NULL) {
        return EC_MEM;
    }
    FILE *fp;
    curr_dirname = dirnames->first;
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
                    if (exit_code != EC_OK) {
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
    destroyStringList(&dirnames);

    // For debug purposes
//    printf("Worker%d with pid %d has successfully loaded the following files:\n", w_id, pid);
//    for (int i = 0; i < doc_count; i++) {
//        printf("  File %d: %s\n", i, docnames[i]);
//        for (int j = 0; j < doclines[i]; j++) {
//            printf("    %d %s\n", j, docs[i][j]);
//        }
//    }

    int strings_matched = 0;
    /// TODO sNprintf? N
    char *logfile;      /// change to [PATH_MAX + 1]
    asprintf(&logfile, "%s/Worker%d", LOGPATH, pid);
    FILE *logfp = fopen(logfile, "w");
    if (logfp < 0) {
        perror("fopen");
        return EC_FILE;
    }
    char *command;
    while (1) {
        pause();
        if (read(fd0, msgbuf, BUFSIZ) < 0) {      // should only be one line
            perror("Error reading from pipe");
            return EC_PIPE;
        }
        strtok(msgbuf, "\n");     // remove trailing newline character
        command = strtok(msgbuf, " \t");
        if (!strcmp(command, cmds[0])) {          // search
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL || !strcmp(keyword, "-d")) {
                exit_code = EC_UNKNOWN;
                break;
            }
            StringList *terms = createStringList();
            if (terms == NULL) {
                return EC_MEM;
            }
            if (appendStringListNode(terms, keyword) != EC_OK) {
                return EC_MEM;
            }
            keyword = strtok(NULL, " \t");
            while (keyword != NULL) {
                if (appendStringListNode(terms, keyword) != EC_OK) {
                    return EC_MEM;
                }
                keyword = strtok(NULL, " \t");
            }
            StringListNode *currTerm = terms->first;
            while (currTerm != NULL) {
                PostingList *keywordPostingList = getPostingList(trie, currTerm->string);
                if (keywordPostingList == NULL) {     // current term doesn't exist in trie
                    fprintf(logfp, "%s : %s : %s :\n", getCurrentTime(), cmds[0] + 1, currTerm->string);
                    currTerm = currTerm->next;
                    continue;
                }
                strings_matched++;
                fprintf(logfp, "%s : %s : %s", getCurrentTime(), cmds[0] + 1, currTerm->string);
                PostingListNode *currPLNode = keywordPostingList->first;
                while (currPLNode != NULL) {
                    fprintf(logfp, " : %s", docnames[currPLNode->id]);
                    IntListNode *currLine = currPLNode->lines->first;
                    while (currLine != NULL) {
                        //if (w_id == 2) sleep(1);    ///
                        sprintf(msgbuf, "%d:%s %d %s", w_id, docnames[currPLNode->id], currLine->line, docs[currPLNode->id][currLine->line]);
                        if (write(fd1, msgbuf, BUFSIZ) < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                            perror("Error writing to pipe");
                            return EC_PIPE;
                        }
                        currLine = currLine->next;
                    }
                    currPLNode = currPLNode->next;
                }
                fprintf(logfp, "\n");
                currTerm = currTerm->next;
            }
            sprintf(msgbuf, "$");
            if (write(fd1, msgbuf, BUFSIZ) < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Error writing to pipe");
                return EC_PIPE;
            }
        } else if (!strcmp(command, cmds[1])) {       // maxcount
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL) {
                exit_code = EC_UNKNOWN;
                break;
            }
            PostingList *keywordPostingList = getPostingList(trie, keyword);
            if (keywordPostingList == NULL) {
                sprintf(msgbuf, "%d:0", w_id);
                if (write(fd1, msgbuf, BUFSIZ) < 0) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
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
            sprintf(msgbuf, "%d:%d %s", w_id, max_tf, docnames[max_id]);
            if (write(fd1, msgbuf, BUFSIZ) < 0) {
                perror("Error writing to pipe");
                return EC_PIPE;
            }
            fprintf(logfp, "%s : %s : %s : %s\n", getCurrentTime(), cmds[1] + 1, keyword, docnames[max_id]);
        } else if (!strcmp(command, cmds[2])) {       // mincount
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL) {
                exit_code = EC_UNKNOWN;
                break;
            }
            PostingList *keywordPostingList = getPostingList(trie, keyword);
            if (keywordPostingList == NULL) {
                sprintf(msgbuf, "%d:0", w_id);
                if (write(fd1, msgbuf, BUFSIZ) < 0) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
                fprintf(logfp, "%s : %s : %s :\n", getCurrentTime(), cmds[1] + 1, keyword);
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
            sprintf(msgbuf, "%d:%d %s", w_id, min_tf, docnames[min_id]);
            if (write(fd1, msgbuf, BUFSIZ) < 0) {
                perror("Error writing to pipe");
                return EC_PIPE;
            }
            fprintf(logfp, "%s : %s : %s : %s\n", getCurrentTime(), cmds[1] + 1, keyword, docnames[min_id]);
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
                total_chars += atoi(strtok(buffer, " \t"));
                total_words += atoi(strtok(NULL, " \t"));
                total_lines += atoi(strtok(NULL, " \t"));
                pclose(pp);
            }
            sprintf(msgbuf, "%d:%d %d %d", w_id, total_chars, total_words, total_lines);
            if (write(fd1, msgbuf, BUFSIZ) < 0) {
                perror("Error writing to pipe");
                return EC_PIPE;
            }
            fprintf(logfp, "%s : %s : %d : %d : %d\n", getCurrentTime(), cmds[3] + 1, total_chars, total_words, total_lines);
        } else if (!strcmp(command, cmds[5])) {       // exit
            /// TODO count total strings found
            printf("Worker%d strings matched: %d\n", w_id, strings_matched);
            break;
        } else {        // shouldn't ever get here
            exit_code = EC_UNKNOWN;
            break;
        }
        buffer = bufferptr;
    }
    if (exit_code == EC_UNKNOWN) {
        fprintf(stderr, "Illegal command '%s' arrived to Worker%d with pid %d. The worker will now terminate.\n", command, w_id, pid);
    }

    if (bufferptr != NULL) {
        free(bufferptr);
    }
    if (fclose(logfp) < 0) {
        perror("fclose");
    }
    if (close(fd0) < 0 || close(fd1) < 0) {
        perror("Error opening pipes");
    }
    deleteTrie(&trie);
    for (int i = 0; i < doc_count; i++) {
        for (int j = 0; j < doclines[i]; j++) {
            free(docs[i][j]);
        }
        free(docs[i]);
    }
    printf("Worker%d with pid %d has exited.\n", w_id, pid);
    return exit_code;
}