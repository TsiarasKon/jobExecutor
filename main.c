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

int makeProgramDirs(int w);
int getNextIncomplete(const int completed[], int w);

void nothing_handler(int signum) {
    // Its only purpose is to unpause the workers
}

int timeout = 0;
void timeout_handler(int signum) {
    timeout = 1;
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
    if ((exit_code = makeProgramDirs(w)) != EC_OK) {
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
        /// TODO clear pipes after interrupt
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

    signal(SIGALRM, timeout_handler);

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
            // TODO accept "-d" as term
            // Validating search query:
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL || !strcmp(keyword, "-d")) {
                fprintf(stderr, "Invalid use of '/search': At least one query term is required.\n");
                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
                continue;
            }
            keyword = strtok(NULL, " \t");
            while (keyword != NULL && (strcmp(keyword, "-d") != 0)) {
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
            int deadline = atoi(keyword);
            if (deadline < 0) {
                fprintf(stderr, "Invalid use of '/search': Negative deadline.\n");
                continue;
            }
            // Search query is valid so we pass it to the workers:
            for (int w_id = 0; w_id < w; w_id++) {
                kill(pids[w_id], SIGCONT);      // signal workers to unpause
                if (write(fd0s[w_id], msgbuf, BUFSIZ) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
            }
            int completed[w];
            StringList *worker_results[w];
            int w_id = 0;
            for (w_id = 0; w_id < w; w_id++) {
                completed[w_id] = 0;
                if ((worker_results[w_id] = createStringList()) == NULL) {
                    return EC_MEM;
                }
            }
            timeout = 0;
            if (deadline != 0) {
                alarm((unsigned int) deadline);
            }
            while ((w_id = getNextIncomplete(completed, w)) != -1 && timeout == 0) {
                if (poll(pfd1s, (nfds_t) w, -1) < 0) {
                    if (errno == EINTR) {       // timed out
                        break;
                    }
                    perror("poll");
                    return EC_PIPE;
                }
                while (completed[w_id] == 0 && w_id < w) {
                    if (pfd1s[w_id].revents & POLLIN) {     // we can read from w_id
                        if (read(pfd1s[w_id].fd, msgbuf, BUFSIZ) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        if (*msgbuf == '$') {
                            completed[w_id] = 1;
                        } else {
                            appendStringListNode(worker_results[w_id], msgbuf);
                        }
                    }
                    w_id++;
                }
            }
            errno = 0;
            // Print results of workers who completed before timeout:
            for (w_id = 0; w_id < w; w_id++) {
                if (completed[w_id]) {
                    StringListNode *current = worker_results[w_id]->first;
                    while (current != NULL) {
                        printf("%s\n", current->string);
                        current = current->next;
                    }
                }
                destroyStringList(&worker_results[w_id]);
            }


        } else if (!strcmp(command, cmds[1])) {       // maxcount
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL) {
                fprintf(stderr, "Invalid use of '/maxcount' - Type '/help' to see the correct syntax.\n");
                continue;
            }
            for (int w_id = 0; w_id < w; w_id++) {
                kill(pids[w_id], SIGCONT);      // signal workers to unpause
                if (write(fd0s[w_id], msgbuf, BUFSIZ) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
            }
            int worker_tf, curr_max_tf = -1;
            char worker_docname[PATH_MAX + 1], curr_max_docname[PATH_MAX + 1];
            int completed[w];
            int w_id;
            for (w_id = 0; w_id < w; w_id++) {
                completed[w_id] = 0;
            }
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
                        completed[w_id] = 1;
                        buffer = strtok(msgbuf, " ");
                        worker_tf = atoi(buffer);
                        if (worker_tf == 0 || worker_tf < curr_max_tf) {
                            continue;
                        }
                        strcpy(worker_docname, strtok(NULL, " "));
                        if (worker_docname == NULL) {
                            fprintf(stderr, "A worker delivered a corrupted message");
                            return EC_UNKNOWN;
                        }
                        if (worker_tf == curr_max_tf && strcmp(curr_max_docname, worker_docname) < 0) {
                            continue;
                        }
                        // Else the current worker has the new max:
                        curr_max_tf = worker_tf;
                        strcpy(curr_max_docname, worker_docname);
                    }
                    w_id++;
                }
            }
            if (curr_max_tf == -1) {
                printf("'%s' doesn't exist in any of the docs.\n", keyword);
            } else {
                printf("'%s' appears the most (%d times) in \"%s\".\n", keyword, curr_max_tf, curr_max_docname);
            }
        } else if (!strcmp(command, cmds[2])) {       // mincount
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL) {
                fprintf(stderr, "Invalid use of '/mincount' - Type '/help' to see the correct syntax.\n");
                continue;
            }
            for (int w_id = 0; w_id < w; w_id++) {
                kill(pids[w_id], SIGCONT);      // signal workers to unpause
                if (write(fd0s[w_id], msgbuf, BUFSIZ) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
            }
            int worker_tf, curr_min_tf = -1;
            char worker_docname[PATH_MAX + 1], curr_min_docname[PATH_MAX + 1];
            int completed[w];
            int w_id;
            for (w_id = 0; w_id < w; w_id++) {
                completed[w_id] = 0;
            }
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
                        completed[w_id] = 1;
                        buffer = strtok(msgbuf, " ");
                        worker_tf = atoi(buffer);
                        if (worker_tf == 0 || (worker_tf > curr_min_tf && curr_min_tf != -1)) {
                            continue;
                        }
                        strcpy(worker_docname, strtok(NULL, " "));
                        if (worker_docname == NULL) {
                            fprintf(stderr, "A worker delivered a corrupted message");
                            return EC_UNKNOWN;
                        }
                        if (worker_tf == curr_min_tf && strcmp(curr_min_docname, worker_docname) < 0) {
                            continue;
                        }
                        // Else the current worker has the new min:
                        curr_min_tf = worker_tf;
                        strcpy(curr_min_docname, worker_docname);
                    }
                    w_id++;
                }
            }
            if (curr_min_tf == -1) {
                printf("'%s' doesn't exist in any of the docs.\n", keyword);
            } else {
                printf("'%s' appears the least (%d times) in \"%s\".\n", keyword, curr_min_tf, curr_min_docname);
            }
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
            int w_id;
            for (w_id = 0; w_id < w; w_id++) {
                completed[w_id] = 0;
            }
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
                        completed[w_id] = 1;
                        total_chars += atoi(strtok(msgbuf, " "));
                        total_words += atoi(strtok(NULL, " "));
                        total_lines += atoi(strtok(NULL, " "));
                    }
                    w_id++;
                }
            }
            printf("Total bytes: %d\n", total_chars);
            printf("Total words: %d\n", total_words);
            printf("Total lines: %d\n", total_lines);
        } else if (!strcmp(command, cmds[4])) {
            printf("Available commands (use without quotes):\n");
            printf(" '/search word1 word2 ... -d deadline' for a list of the files that include the given words, along with the lines where they appear.\n");
            printf("    Results will be printed within the seconds given as an integer (deadline).\n");
            printf("    To wait for all the results without a deadline use '-d 0'.\n");
            printf(" '/maxcount word' for the file where the given word appears the most.\n");
            printf(" '/mincount word' for the file where the given word appears the least (but at least once).\n");
            printf(" '/wc' for the number of characters (bytes), words and lines of every file.\n");
            printf(" '/help' for the list you're seeing right now.\n");
            printf(" '/exit' to terminate this program.\n");
            printf(" '/exit -l' to terminate this program and also delete all log files.\n");
        } else if (!strcmp(command, cmds[5])) {       // exit
            command = strtok(NULL, " \t");
            for (int w_id = 0; w_id < w; w_id++) {
                kill(pids[w_id], SIGCONT);      // signal workers to unpause
                if (write(fd0s[w_id], msgbuf, BUFSIZ) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
            }
            int completed[w];
            int w_id;
            for (w_id = 0; w_id < w; w_id++) {
                completed[w_id] = 0;
            }
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
                        completed[w_id] = 1;
                        printf("Worker%d found %d search strings.\n", w_id, atoi(msgbuf));
                    }
                    w_id++;
                }
            }
            for (w_id = 0; w_id < w; w_id++) {
                waitpid(pids[w_id], NULL, 0);
            }
            // Deleting logs, if "-l" was specified:
            if (command != NULL && !strcmp(command, "-l")) {
                DIR *dirp;
                struct dirent *curr_dirent;
                char curr_dirname[PATH_MAX + 1], curr_filename[PATH_MAX + 1];
                for(w_id = 0; w_id < w; w_id++) {
                    sprintf(curr_dirname, "%s/Worker%d", LOGPATH, w_id);
                    if ((dirp = opendir(curr_dirname)) == NULL) {
                        perror("Error opening log directory for deletion");
                        continue;
                    }
                    while ((curr_dirent = readdir(dirp))) {
                        if ((strcmp(curr_dirent->d_name, ".") != 0) && (strcmp(curr_dirent->d_name, "..") != 0)) {
                            sprintf(curr_filename, "%s/%s", curr_dirname, curr_dirent->d_name);
                            if (unlink(curr_filename) < 0) {
                                perror("Error deleting logfile");
                            }
                        }
                    }
                    if (rmdir(curr_dirname) < 0) {
                        perror("Error deleting log directory");
                    }
                }
                if (rmdir(LOGPATH) < 0) {
                    perror("Error deleting log directory");
                }
            }
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

int makeProgramDirs(int w) {
    if (mkdir(PIPEPATH, 0777) < 0 && errno != EEXIST) {
        perror("mkdir");
        return EC_DIR;
    }
    if (mkdir(LOGPATH, 0777) < 0 && errno != EEXIST) {
        perror("mkdir");
        return EC_DIR;
    }
    char worker_logdir[PATH_MAX + 1];
    for (int w_id = 0; w_id < w; w_id++) {
        sprintf(worker_logdir, "%s/Worker%d", LOGPATH, w_id);
        if (mkdir(worker_logdir, 0777) < 0 && errno != EEXIST) {
            perror("mkdir");
            return EC_DIR;
        }
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