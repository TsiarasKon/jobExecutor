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
#include <poll.h>
#include "trie.h"
#include "paths.h"
#include "util.h"

int worker(int w_id);

int makeProgramDirs(void);
int getNextIncomplete(const int completed[], int w);

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

    FILE *fp;
    if ((fp = fopen(docfile, "r")) == NULL) {
        perror("fopen");
        return EC_FILE;
    }
    size_t bufsize = 128;      // sample size - getline will reallocate memory as needed
    char *buffer = NULL, *bufferptr = NULL;
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

    int exit_code;
    if ((exit_code = makeProgramDirs()) != EC_OK) {
        fprintf(stderr, "Unable to create folders.\n");
        return exit_code;
    }

    signal(SIGCONT, nothing_handler);
    int pids[w];
    struct pollfd pfd1s[w];
    for (int i = 0; i < w; i++) {
        pfd1s[i].events = POLLIN;
    }
    char fifo0[PATH_MAX], fifo1[PATH_MAX];
    for(int w_id = 0; w_id < w; w_id++) {
        sprintf(fifo0, "%s/Worker%d_0", PIPEPATH, w_id);
        sprintf(fifo1, "%s/Worker%d_1", PIPEPATH, w_id);
        if (((mkfifo(fifo0, 0666) == -1) || (mkfifo(fifo1, 0666) == -1)) && (errno != EEXIST)) {
            perror("Error creating pipes");
            return EC_PIPE;
        }
        pfd1s[w_id].fd = open(fifo1, O_RDONLY | O_NONBLOCK);
        if (pfd1s[w_id].fd < 0) {
            perror("Error opening pipe");
            return EC_PIPE;
        }
        pids[w_id] = fork();
        if (pids[w_id] < 0) {
            perror("fork");
            return EC_FORK;
        } else if (pids[w_id] == 0) {
            printf("Created Worker%d with pid %d\n", w_id, getpid());
            if (close(pfd1s[w_id].fd) < 0) {
                perror("Worker failed to close pipe after forking");
                return EC_PIPE;
            }
            return worker(w_id);
        }
    }

    char *dirnames[dirs_num];
    if ((fp = fopen(docfile, "r")) == NULL) {
        perror("fopen");
        return EC_FILE;
    }
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

    int fd0s[w];
    for(int w_id = 0; w_id < w; w_id++) {
        sprintf(fifo0, "%s/Worker%d_0", PIPEPATH, w_id);
        fd0s[w_id] = open(fifo0, O_WRONLY);
        if (fd0s[w_id] < 0) {
            perror("Error opening pipe");
            return EC_PIPE;
        }
    }

    char pipebuffer[BUFSIZ];
    for(int w_id = 0; w_id < w; w_id++) {
        for (int curr_dir = w_id; curr_dir < dirs_num; curr_dir += w) {
            strcpy(pipebuffer, dirnames[curr_dir]);
            if (write(fd0s[w_id], pipebuffer, BUFSIZ) == -1) {
                perror("Error writing to pipe");
                return EC_PIPE;
            }
        }
        strcpy(pipebuffer, "$");
        if (write(fd0s[w_id], pipebuffer, BUFSIZ) == -1) {
            perror("Error writing to pipe");
            return EC_PIPE;
        }
    }

    /// TODO force BUSIZ with check
    char *command, msgbuf[BUFSIZ + 1];
    while (1) {
        printf("\n");
        // Until "/exit" is given, read current line and attempt to execute it as a command
        printf("Type a command:\n");
        getline(&buffer, &bufsize, stdin);
        strcpy(msgbuf, buffer);
        bufferptr = buffer;
        strtok(buffer, "\n");     // remove trailing newline character
        command = strtok(buffer, " \t");
        if (!strcmp(command, cmds[0])) {          // search
            //char completed[w] = {0};
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
                kill(pids[w_id], SIGCONT);      // signal workers to unpause
                if (write(fd0s[w_id], msgbuf, BUFSIZ) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
            }
            int total_chars = 0, total_words = 0, total_lines = 0;
            int completed[w];
            for (int i = 0; i < w; i++) {
                completed[i] = 0;
            }
            int w_id;
            while ((w_id = getNextIncomplete(completed, w)) != -1) {
                if (poll(pfd1s, (nfds_t) w, -1) < 0) {
                    perror("poll");
                    return EC_PIPE;
                }
                while (completed[w_id] == 0 && w_id < w) {
                    if (pfd1s[w_id].revents & POLLIN) {     // we can read from w_id
                        if (read(pfd1s[w_id].fd, msgbuf, BUFSIZ) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        buffer = strchr(msgbuf, ':') + 1;       // ignore w_id
                        total_chars += atoi(strtok(buffer, " "));
                        total_words += atoi(strtok(NULL, " "));
                        total_lines += atoi(strtok(NULL, " "));
                        completed[w_id] = 1;
                    }
                    w_id++;
                }
            }
            printf("Total bytes: %d\n", total_chars);
            printf("Total words: %d\n", total_words);
            printf("Total lines: %d\n", total_lines);
        } else if (!strcmp(command, cmds[4])) {
            printf("Available commands (use without quotes):\n");
            printf(" '/search word1 word2 ... -d sec' for a list of the files that include the given words, along with the lines where they appear. Results will be printed within the seconds given as a deadline.\n");
            printf(" '/maxcount word' for the file where the given word appears the most.\n");
            printf(" '/mincount word' for the file where the given word appears the least (but at least once).\n");
            printf(" '/wc' for the number of characters (bytes), words and lines of every file.\n");
            printf(" '/help' for the list you're seeing right now.\n");
            printf(" '/exit' to terminate this program.\n");
        } else if (!strcmp(command, cmds[5])) {       // exit
            for (int w_id = 0; w_id < w; w_id++) {
                kill(pids[w_id], SIGCONT);      // signal workers to unpause
                if (write(fd0s[w_id], msgbuf, BUFSIZ) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
            }
            wait(NULL);
            break;
        } else {
            fprintf(stderr, "Unknown command '%s': Type '/help' for a detailed list of available commands.\n", command);
        }
        buffer = bufferptr;
    }

    for(int w_id = 0; w_id < w; w_id++) {
        if (close(fd0s[w_id]) < 0 || close(pfd1s[w_id].fd) < 0) {
            perror("Error closing pipes");
        }
        sprintf(fifo0, "%s/Worker%d_0", PIPEPATH, w_id);
        sprintf(fifo1, "%s/Worker%d_1", PIPEPATH, w_id);
        if (unlink(fifo0) < 0 || unlink(fifo1) < 0) {
            perror("Error deleting pipes");
        }
    }
    if (rmdir(PIPEPATH) < 0) {
        perror("rmdir");
    }
    if (bufferptr != NULL) {
        free(bufferptr);
    }
    free(docfile);
    return exit_code;
}

int makeProgramDirs(void) {
    if (mkdir(PIPEPATH, 0777) < 0 && errno != EEXIST) {
        perror("mkdir");
        return EC_DIR;
    }
    if (mkdir(LOGPATH, 0777) < 0 && errno != EEXIST) {
        perror("mkdir");
        return EC_DIR;
    }
    return EC_OK;
}

int getNextIncomplete(const int completed[], int w) {
    for (int i = 0; i < w; i++) {
        if (completed[i] == 0) {
            return i;
        }
    }
    return -1;
}