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
#include <sys/wait.h>
#include <sys/stat.h>
#include "trie.h"
#include "paths.h"
#include "util.h"

int worker(int w_id);

void nothing_handler(int signum) {
    // Its only purpose is to unpause the workers
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Invalid arguments. Please run ");       ///
        return EC_ARG;
    }
    int w = 0;
    char *docfile = NULL;
    int option;
    while ((option = getopt(argc, argv,"d:w:")) != -1) {
        switch (option) {
            case 'd' :
                docfile = malloc(strlen(optarg) + 1);
                strcpy(docfile, optarg);
                break;
            case 'w' : w = atoi(optarg);
                break;
            default:
                return EC_ARG;
        }
    }
    if (w == 0 || docfile == NULL) {
        fprintf(stderr, "Invalid arguments. Please run ");       ///
        return EC_ARG;
    }
    int exit_code = 0;      ///

    FILE *fp = fopen(docfile, "r");
    size_t bufsize = 128;      // sample size - getline will reallocate memory as needed
    char *buffer = NULL, *bufferptr = NULL;
    if (fp == NULL) {
        perror("Failed to open file");
        return EC_FILE;
    }
    int dirs_num = 0;
    while (getline(&buffer, &bufsize, fp) != -1) {
        dirs_num++;
    }
    fclose(fp);
    if (dirs_num == 0) {
        printf("No directories given - Nothing to do here!\n");
        return EC_OK;
    }
    if (dirs_num < w) {     // No reason to create more workers than the number of directories
        w = dirs_num;
    }

    signal(SIGCONT, nothing_handler);
    int pids[w];
    int fds[w][2];
    char fifo0[PATH_MAX], fifo1[PATH_MAX];
    for(int w_id = 0; w_id < w; w_id++) {
        sprintf(fifo0, "%s/Worker%d_0", PIPEPATH, w_id);
        sprintf(fifo1, "%s/Worker%d_1", PIPEPATH, w_id);
        if (((mkfifo(fifo0, 0666) == -1) || (mkfifo(fifo1, 0666) == -1)) && (errno != EEXIST)) {
            perror("Error creating pipes");
            return EC_FIFO;
        }
        pids[w_id] = fork();
        if (pids[w_id] < 0) {
            perror("fork");
            return EC_FORK;
        } else if (pids[w_id] == 0) {
            printf("Created worker #%d with pid:%d\n", w_id, getpid());
            return worker(w_id);
        }
    }

    char *dirnames[dirs_num];
    fp = fopen(docfile, "r");
    DIR *testdir;
    for (int curr_line = 0; curr_line < dirs_num; curr_line++) {
        if (getline(&buffer, &bufsize, fp) == -1) {
            perror("Error");
            return EC_UNKNOWN;
        }
        bufferptr = buffer;
        strtok(buffer, "\n");
        buffer = strtok(buffer, " \t");
        if (!(testdir = opendir(buffer))) {     // couldn't open dir
            perror("Failed to open directory");
            return EC_DIR;
        }
        closedir(testdir);
        dirnames[curr_line] = malloc(strlen(buffer) + 1);
        strcpy(dirnames[curr_line], buffer);
        buffer = bufferptr;
    }
    fclose(fp);
    sleep(2);
    char pipebuffer[BUFSIZ];
    for(int w_id = 0; w_id < w; w_id++) {
        sprintf(fifo0, "%s/Worker%d_0", PIPEPATH, w_id);
        fds[w_id][0] = open(fifo0, O_WRONLY);
        if (fds[w_id][0] < 0) {
            perror("Error opening pipe");
            return EC_FIFO;
        }
        printf("jE: Opened pipe %s for writing\n", fifo0);
        for (int curr_dir = w_id; curr_dir < dirs_num; curr_dir += w) {
            strcpy(pipebuffer, dirnames[curr_dir]);
            if (write(fds[w_id][0], pipebuffer, BUFSIZ) == -1) {
                perror("Error writing to pipe");
                return EC_FIFO;
            }
            //fsync(fds[w_id][0]);
        }
        close(fds[w_id][0]);
    }

    /// TODO force BUSIZ with check
    char *command, commandcopy[BUFSIZ + 1];
    while (1) {
        printf("\n");
        // Until "/exit" is given, read current line and attempt to execute it as a command
        printf("Type a command:\n");
        getline(&buffer, &bufsize, stdin);
        strcpy(commandcopy, buffer);
        bufferptr = buffer;
        strtok(buffer, "\n");     // remove trailing newline character
        command = strtok(buffer, " \t");
        if (!strcmp(command, cmds[0])) {          // search
//            char *keyword = strtok(NULL, " \t");
//            if (keyword == NULL || !strcmp(keyword, "-d")) {
//                fprintf(stderr, "Invalid use of '/search': At least one query term is required.\n");
//                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
//                continue;
//            }
//            StringListNode *first_term = createStringListNode(keyword);
//            StringListNode *last_term = first_term;
//            keyword = strtok(NULL, " \t");
//            while (keyword != NULL && (strcmp(keyword, "-d") != 0)) {
//                last_term->next = createStringListNode(keyword);
//                last_term = last_term->next;
//                keyword = strtok(NULL, " \t");
//            }
//            if (keyword == NULL || (strcmp(keyword, "-d") != 0)) {
//                fprintf(stderr, "Invalid use of '/search': No deadline specified.\n");
//                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
//                continue;
//            }
//            keyword = strtok(NULL, " \t");
//            if (keyword == NULL || !isdigit(*keyword)) {
//                fprintf(stderr, "Invalid use of '/search': Invalid deadline.\n");
//                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
//                continue;
//            }
//            int deadline = atoi(keyword);       ///
//            StringListNode *currTerm = first_term;
//            while (currTerm != NULL) {
//                PostingList *keywordPostingList = getPostingList(trie, currTerm->string);
//                if (keywordPostingList == NULL) {     // current term doesn't exist in trie
//                    fprintf(logfp, "%s : %s : %s :\n", getCurrentTime(), cmds[0] + 1, currTerm->string);
//                    currTerm = currTerm->next;
//                    continue;
//                }
//                fprintf(logfp, "%s : %s : %s", getCurrentTime(), cmds[0] + 1, currTerm->string);
//                PostingListNode *currNode = keywordPostingList->first;
//                while (currNode != NULL) {
//                    fprintf(logfp, " : %s", docnames[currNode->id]);
//                    IntListNode *currLine = currNode->firstline;
//                    while (currLine != NULL) {
//                        printf("%s %d: %s\n", docnames[currNode->id], currLine->line, docs[currNode->id][currLine->line]);
//                        currLine = currLine->next;
//                    }
//                    currNode = currNode->next;
//                }
//                fprintf(logfp, "\n");
//                currTerm = currTerm->next;
//            }
        } else if (!strcmp(command, cmds[1])) {       // maxcount
//            char *keyword = strtok(NULL, " \t");
//            if (keyword == NULL) {
//                fprintf(stderr, "Invalid use of '/maxcount' - Type '/help' to see the correct syntax.\n");
//                continue;
//            }
//            PostingList *keywordPostingList = getPostingList(trie, keyword);
//            if (keywordPostingList == NULL) {
//                printf("'%s' doesn't exist in docs.\n", keyword);
//                fprintf(logfp, "%s : %s : %s :\n", getCurrentTime(), cmds[1] + 1, keyword);
//                continue;
//            }
//            PostingListNode *current = keywordPostingList->first;
//            int max_id = keywordPostingList->first->id;
//            int max_tf = keywordPostingList->first->tf;
//            current = current->next;
//            while (current != NULL) {
//                if (current->tf > max_tf || (current->tf == max_tf && (strcmp(docnames[current->id], docnames[max_id]) < 0))) {
//                    max_id = current->id;
//                    max_tf = current->tf;
//                }
//                current = current->next;
//            }
//            printf("'%s' appears the most in \"%s\".\n", keyword, docnames[max_id]);
//            fprintf(logfp, "%s : %s : %s : %s\n", getCurrentTime(), cmds[1] + 1, keyword, docnames[max_id]);
        } else if (!strcmp(command, cmds[2])) {       // mincount
//            char *keyword = strtok(NULL, " \t");
//            if (keyword == NULL) {
//                fprintf(stderr, "Invalid use of '/mincount' - Type '/help' to see the correct syntax.\n");
//                continue;
//            }
//            PostingList *keywordPostingList = getPostingList(trie, keyword);
//            if (keywordPostingList == NULL) {
//                printf("'%s' doesn't exist in docs.\n", keyword);
//                fprintf(logfp, "%s : %s : %s :\n", getCurrentTime(), cmds[2] + 1, keyword);
//                continue;
//            }
//            PostingListNode *current = keywordPostingList->first;
//            int min_id = keywordPostingList->first->id;
//            int min_tf = keywordPostingList->first->tf;
//            current = current->next;
//            while (current != NULL) {
//                if (current->tf < min_tf || (current->tf == min_tf && (strcmp(docnames[current->id], docnames[min_id]) < 0))) {
//                    min_id = current->id;
//                    min_tf = current->tf;
//                }
//                current = current->next;
//            }
//            printf("'%s' appears the least in \"%s\".\n", keyword, docnames[min_id]);
//            fprintf(logfp, "%s : %s : %s : %s\n", getCurrentTime(), cmds[2] + 1, keyword, docnames[min_id]);
        } else if (!strcmp(command, cmds[3])) {       // wc
            for (int w_id = 0; w_id < w; w_id++) {
                kill(pids[w_id], SIGCONT);
                sprintf(fifo0, "%s/Worker%d_0", PIPEPATH, w_id);
                fds[w_id][0] = open(fifo0, O_WRONLY);
                if (fds[w_id][0] < 0) {
                    perror("Error opening pipe");
                    return EC_FIFO;
                }
                printf("jE: %s", commandcopy);
                if (write(fds[w_id][0], commandcopy, BUFSIZ) == -1) {
                    perror("Error writing to pipe");
                    return EC_FIFO;
                }
                close(fds[w_id][0]);
            }
//            int total_chars = 0, total_words = 0, total_lines = 0;
//            FILE *pp;
//            char command_wc[PATH_MAX + 6];
//            for (curr_doc = 0; curr_doc < doc_count; curr_doc++) {
//                sprintf(command_wc, "wc \"%s\"", docnames[curr_doc]);
//                pp = popen(command_wc, "r");
//                if (pp == NULL) {
//                    perror("Failed to run command");
//                    return EC_CMD;
//                }
//                buffer = bufferptr;
//                if (getline(&buffer, &bufsize, pp) == -1) {
//                    perror("Failed to run command");
//                    return EC_CMD;
//                }
//                //printf("%s\n", buffer);
//                total_chars += atoi(strtok(buffer, " \t"));
//                total_words += atoi(strtok(NULL, " \t"));
//                total_lines += atoi(strtok(NULL, " \t"));
//                pclose(pp);
//            }
//            printf("Worker bytes: %d\n", total_chars);
//            printf("Worker words: %d\n", total_words);
//            printf("Worker lines: %d\n", total_lines);
        } else if (!strcmp(command, cmds[4])) {
            printf("Available commands (use without quotes):\n");
            printf(" '/search word1 word2 ... -d sec' for a list of the files that include the given words, along with the lines where they appear. Results will be printed within the seconds given as a deadline.\n");
            printf(" '/maxcount word' for the file where the given word appears the most.\n");
            printf(" '/mincount word' for the file where the given word appears the least (but at least once).\n");
            printf(" '/wc' for the number of characters (bytes), words and lines of every file.\n");
            printf(" '/help' for the list you're seeing right now.\n");
            printf(" '/exit' to terminate this program.\n");
        } else if (!strcmp(command, cmds[5])) {       // exit
            /// terminate all
            break;
        } else {
            fprintf(stderr, "Unknown command '%s': Type '/help' for a detailed list of available commands.\n", command);
        }
        buffer = bufferptr;
    }

    /// wait()

    if (bufferptr != NULL) {
        free(bufferptr);
    }
    free(docfile);
    return exit_code;
}
