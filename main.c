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

int w = 0;

int worker(int w_id);

int makeProgramDirs(void);

static int timeout = 0;
void timeout_handler(int signum) {
    timeout = 1;
}

void nothing_handler(int signum) {
    // Used only to unpause
}

static int executorKilled = 0;
void executor_cleanup(int signum) {
    executorKilled = 1;
}

pid_t *pidsptr;
int *child_aliveptr;    // global pointer so as to know which worker needs replacing
void child_handler(int signum);

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Invalid arguments. Please run \"$ ./jobExecutor -d docfile -w numWorkers\"\n");
        return EC_ARG;
    }
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
                fprintf(stderr, "Invalid arguments. Please run \"$ ./jobExecutor -d docfile -w numWorkers\"\n");
                return EC_ARG;
        }
    }
    if (w == 0 || docfile == NULL) {
        fprintf(stderr, "Invalid arguments. Please run \"$ ./jobExecutor -d docfile -w numWorkers\"\n");
		if (docfile != NULL) {
			free(docfile);
		}
        return EC_ARG;
    }

    // Count number of directories in docfile:
    FILE *fp;
    if ((fp = fopen(docfile, "r")) == NULL) {
        perror("fopen");
		free(docfile);
        return EC_FILE;
    }
    size_t bufsize = 128;      // sample size - getline will reallocate memory as needed
    char *buffer = NULL, *bufferptr = NULL;
    int dirs_num = 0;
    DIR *testdir;
    while (getline(&buffer, &bufsize, fp) != -1) {
        if (buffer[0] == '\n')  {     // ignore empty lines
            continue;
        }
        bufferptr = buffer;
        strtok(buffer, "\r\n");
        buffer = strtok(buffer, " \t");
        if (!(testdir = opendir(buffer))) {     // couldn't open dir
            fprintf(stderr, "Failed to open directory \"%s\"", buffer);
			perror(" ");
			buffer = bufferptr;
			continue;
        }
        closedir(testdir);
        dirs_num++;
		buffer = bufferptr;
    }
    fclose(fp);
    if (dirs_num == 0) {
        printf("No directories given - Nothing to do here!\n");
		free(docfile);
		if (bufferptr != NULL) {
			free(bufferptr);
		}
        return EC_OK;
    }
    if (dirs_num < w) {     // No reason to create more workers than the number of directories
        w = dirs_num;
    }

    int exit_code;
    if ((exit_code = makeProgramDirs()) != EC_OK) {
        fprintf(stderr, "Unable to create folders.\n");
		free(docfile);
        return exit_code;
    }

    pid_t pids[w];
    pidsptr = pids;
    int child_alive[w];
    child_aliveptr = child_alive;
    struct pollfd pfd1s[w];     // pollfds are needed for polling from multiple pipes later on
    for (int i = 0; i < w; i++) {
        pfd1s[i].events = POLLIN;
    }
    char fifo0_name[PATH_MAX], fifo1_name[PATH_MAX];
    for(int w_id = 0; w_id < w; w_id++) {
        sprintf(fifo0_name, "%s/Worker%d_0", PIPEPATH, w_id);
        if (mkfifo(fifo0_name, 0666) == -1) {
            if (errno == EEXIST) {      // if fifo already existed we delete it (for good measure) and recreate it
                unlink(fifo0_name);
                if (mkfifo(fifo0_name, 0666) == -1) {
                    perror("Error creating pipe");
					free(docfile);
                    return EC_PIPE;
                }
            } else {
                perror("Error creating pipe");
				free(docfile);
                return EC_PIPE;
            }
        }
        sprintf(fifo1_name, "%s/Worker%d_1", PIPEPATH, w_id);
        if (mkfifo(fifo1_name, 0666) == -1) {
            if (errno == EEXIST) {      // if fifo already existed we delete it (for good measure) and recreate it
                unlink(fifo1_name);
                if (mkfifo(fifo1_name, 0666) == -1) {
                    perror("Error creating pipe");
					free(docfile);
                    return EC_PIPE;
                }
            } else {
                perror("Error creating pipe");
				free(docfile);
                return EC_PIPE;
            }
        }
        // Fork w workers:
        pids[w_id] = fork();
        if (pids[w_id] < 0) {
            perror("fork");
			free(docfile);
            return EC_FORK;
        } else if (pids[w_id] == 0) {
            printf("Created Worker%d with pid %d\n", w_id, getpid());
            free(buffer);
            free(docfile);
            return worker(w_id);
        }
        pfd1s[w_id].fd = open(fifo1_name, O_RDONLY);
        if (pfd1s[w_id].fd < 0) {
            perror("Error opening pipe");
			free(docfile);
            return EC_PIPE;
        }
        child_alive[w_id] = 1;
    }

    // Read directories from docfile and save them in dirnames[]:
    char *dirnames[dirs_num];
    if ((fp = fopen(docfile, "r")) == NULL) {
        perror("fopen");
		free(docfile);
        return EC_FILE;
    }
    for (int curr_line = 0; curr_line < dirs_num; curr_line++) {
        if (getline(&buffer, &bufsize, fp) == -1) {
            perror("Error");
            return EC_UNKNOWN;
        }
        if (buffer[0] == '\n')  {     // ignore empty lines
            curr_line--;    // empty lines shouldn't count as actual lines
            continue;
        }
         bufferptr = buffer;
        strtok(buffer, "\r\n");
        buffer = strtok(buffer, " \t");
        if (!(testdir = opendir(buffer))) {     // invalid directory
			buffer = bufferptr;
			curr_line--;
			continue;
        }
        closedir(testdir);
        dirnames[curr_line] = malloc(strlen(buffer) + 1);
        strcpy(dirnames[curr_line], buffer);
        buffer = bufferptr;
    }
    fclose(fp);
	free(docfile);

    int fd0s[w];
    for(int w_id = 0; w_id < w; w_id++) {
        sprintf(fifo0_name, "%s/Worker%d_0", PIPEPATH, w_id);
        fd0s[w_id] = open(fifo0_name, O_WRONLY);
        if (fd0s[w_id] < 0) {
            perror("Error opening pipe");
            return EC_PIPE;
        }
    }

    // Send the directories to worker
    char pathbuf[PATH_MAX + 1];
    for(int w_id = 0; w_id < w; w_id++) {
        for (int curr_dir = w_id; curr_dir < dirs_num; curr_dir += w) {
            memset(pathbuf, 0, PATH_MAX + 1);
            strcpy(pathbuf, dirnames[curr_dir]);
            if (write(fd0s[w_id], pathbuf, PATH_MAX + 1) == -1) {
                perror("Error writing to pipe");
                return EC_PIPE;
            }
        }
        strcpy(pathbuf, "$");
        if (write(fd0s[w_id], pathbuf, PATH_MAX + 1) == -1) {
            perror("Error writing to pipe");
            return EC_PIPE;
        }
    }

    signal(SIGCHLD, child_handler);
    signal(SIGCONT, nothing_handler);
    signal(SIGALRM, timeout_handler);
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = executor_cleanup;
    sigaction(SIGINT,  &act, 0);
    sigaction(SIGTERM, &act, 0);
    sigaction(SIGQUIT, &act, 0);

    char *command, *writebuf = NULL, *readbuf = NULL, *readbufptr;
    size_t msgsize;
    while (!executorKilled) {             // main program loop
        printf("\n");
        // Until "/exit" is given, read current line and attempt to execute it as a command
        printf("Type a command:\n");
        getline(&buffer, &bufsize, stdin);
        if (executorKilled) {
            break;
        }
        int tw_id;
        while ((tw_id = getNextZero(child_alive, w)) >= 0) {       // check if any workers have died and replace them here
            pid_t terminated_worker_pid = pids[tw_id];
            if (terminated_worker_pid != 0) {
                pids[tw_id] = fork();
                if (pids[tw_id] < 0) {
                    perror("fork");
                    return EC_FORK;
                } else if (pids[tw_id] == 0) {
                    printf("Created new Worker%d with pid %d\n", tw_id, getpid());
                    // free jobExecutor's resources:
                    for (int w_id = 0; w_id < w; w_id++) {
                        if (close(fd0s[w_id]) < 0 || close(pfd1s[w_id].fd) < 0) {
                            perror("Error closing pipes");
                        }
                    }
                    if (bufferptr != NULL) {
                        free(bufferptr);
                    }
                    if (writebuf != NULL) {
                        free(writebuf);
                    }
                    if (readbuf != NULL) {
                        free(readbuf);
                    }
                    for (int i = 0; i < dirs_num; i++) {
                        free(dirnames[i]);
                    }
                    return worker(tw_id);
                }
                alarm(1);       // just in case worker sends the signal before jobExecutor calls pause()
                pause();        // wait for new worker to open its pipe before writing to it
                for (int curr_dir = tw_id; curr_dir < dirs_num; curr_dir += w) {     // send its directories
                    memset(pathbuf, 0, PATH_MAX + 1);
                    strcpy(pathbuf, dirnames[curr_dir]);
                    if (write(fd0s[tw_id], pathbuf, PATH_MAX + 1) == -1) {
                        perror("Error writing to pipe");
                        return EC_PIPE;
                    }
                }
                strcpy(pathbuf, "$");
                if (write(fd0s[tw_id], pathbuf, PATH_MAX + 1) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
            }
            child_alive[tw_id] = 1;
        }
        msgsize = strlen(buffer) + 1;
        writebuf = realloc(writebuf, msgsize);
        if (writebuf == NULL) {
            perror("realloc");
            return EC_MEM;
        }
        strcpy(writebuf, buffer);
        bufferptr = buffer;
        strtok(buffer, "\r\n");     // remove trailing newline character
        command = strtok(buffer, " \t");
        if (!strcmp(command, cmds[0])) {          // search
            // Validating search query:
            char *keyword = strtok(NULL, " \t");
            char *prev_keyword1 = NULL, *prev_keyword2 = NULL;
            if (keyword == NULL) {
                fprintf(stderr, "Invalid use of '/search': At least one query term is required.\n");
                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
                continue;
            }
            prev_keyword1 = keyword;
            keyword = strtok(NULL, " \t");
            if (keyword == NULL) {
                fprintf(stderr, "Invalid use of '/search': No deadline specified.\n");
                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
                continue;
            }
            prev_keyword2 = prev_keyword1;
            prev_keyword1 = keyword;
            keyword = strtok(NULL, " \t");
            if (keyword == NULL) {
                fprintf(stderr, "Invalid use of '/search': Type '/help' to see the correct syntax.\n");
                continue;
            }
            sprintf(writebuf, "/search");
            char *tempbuf;
            while (keyword != NULL) {
                asprintf(&tempbuf, "%s %s", writebuf, prev_keyword2);
                strcpy(writebuf, tempbuf);
                prev_keyword2 = prev_keyword1;
                prev_keyword1 = keyword;
                keyword = strtok(NULL, " \t");
                free(tempbuf);
            }
            if ((strcmp(prev_keyword2, "-d")) != 0) {
                fprintf(stderr, "Invalid use of '/search': No deadline specified.\n");
                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
                continue;
            }
            if (!isdigit(*prev_keyword1)) {
                fprintf(stderr, "Invalid use of '/search': Invalid deadline.\n");
                fprintf(stderr, "  Type '/help' to see the correct syntax.\n");
                continue;
            }
            int deadline = atoi(prev_keyword1);
            if (deadline < 0) {
                fprintf(stderr, "Invalid use of '/search': Negative deadline.\n");
                continue;
            }
            // Search query is valid so we pass it to the workers:
            for (int w_id = 0; w_id < w; w_id++) {
                if (write(fd0s[w_id], &msgsize, sizeof(size_t)) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
                if (write(fd0s[w_id], writebuf, msgsize) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
            }
            int completed[w];
            StringList *worker_results[w];      // save each worker's result in a list
            int w_id = 0;
            for (w_id = 0; w_id < w; w_id++) {
                completed[w_id] = 0;
                if ((worker_results[w_id] = createStringList()) == NULL) {
                    return EC_MEM;
                }
            }
            int w_responded = 0;
            timeout = 0;
            if (deadline != 0) {
                alarm((unsigned int) deadline);   // when the time for search is up jobExecutor will get a SIGALRM
            }
            while ((w_id = getNextZero(completed, w)) != -1 && timeout == 0) {
                if (poll(pfd1s, (nfds_t) w, -1) < 0 && errno != EINTR) {
                    if (errno == EINTR) {       // timed out
                        break;
                    }
                    perror("poll");
                    return EC_PIPE;
                }
                while (w_id < w) {
                    if ((pfd1s[w_id].revents & POLLIN) && completed[w_id] == 0) {     // we can read from w_id
                        if (read(pfd1s[w_id].fd, &msgsize, sizeof(size_t)) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        readbuf = realloc(readbuf, msgsize);
                        if (readbuf == NULL) {
                            perror("realloc");
                            return EC_MEM;
                        }
                        if (read(pfd1s[w_id].fd, readbuf, msgsize) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        if (*readbuf == '$') {
                            completed[w_id] = 1;
                            w_responded++;
                        } else {
                            appendStringListNode(worker_results[w_id], readbuf);
                        }
                    }
                    ++w_id;
                }
            }
            errno = 0;
            if (timeout) {
                for (w_id = 0; w_id < w; w_id++) {
                    if (!completed[w_id]) {
                        kill(pids[w_id], SIGUSR1);      // signal workers to stop executing the search query
                        printf("Worker%d failed to respond on time.\n", w_id);
                    }
                }
            }
            printf("Results from %d out of %d Workers:\n", w_responded, w);
            int results = 0;
            for (w_id = 0; w_id < w; w_id++) {
                if (completed[w_id] && worker_results[w_id]->first != NULL) {
                    results = 1;
                    break;
                }
            }
            if (!results) {
                printf("The given word(s) could not be found in any of the searched docs.\n");
                for (w_id = 0; w_id < w; w_id++) {
                    deleteStringList(&worker_results[w_id]);
                }
            } else {        // Print results of workers who completed before timeout:
                for (w_id = 0; w_id < w; w_id++) {
                    if (completed[w_id]) {
                        StringListNode *current = worker_results[w_id]->first;
                        while (current != NULL) {
                            printf("%s\n", current->string);
                            current = current->next;
                        }
                    }
                    deleteStringList(&worker_results[w_id]);
                }
            }
            // Read what's left from incomplete workers in order to clear the pipes:
            while ((w_id = getNextZero(completed, w)) != -1) {
                if (poll(pfd1s, (nfds_t) w, -1) < 0 && errno != EINTR) {
                    perror("poll");
                    return EC_PIPE;
                }
                while (w_id < w) {
                    if ((pfd1s[w_id].revents & POLLIN) && completed[w_id] == 0) {     // we can read from w_id
                        if (read(pfd1s[w_id].fd, &msgsize, sizeof(size_t)) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        readbuf = realloc(readbuf, msgsize);
                        if (readbuf == NULL) {
                            perror("realloc");
                            return EC_MEM;
                        }
                        if (read(pfd1s[w_id].fd, readbuf, msgsize) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        if (*readbuf == '$') {
                            completed[w_id] = 1;
                        }
                    }
                    w_id++;
                }
            }
        } else if (!strcmp(command, cmds[1])) {       // maxcount
            char *keyword = strtok(NULL, " \t");
            if (keyword == NULL) {
                fprintf(stderr, "Invalid use of '/maxcount' - Type '/help' to see the correct syntax.\n");
                continue;
            }
            for (int w_id = 0; w_id < w; w_id++) {
                if (write(fd0s[w_id], &msgsize, sizeof(size_t)) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
                if (write(fd0s[w_id], writebuf, msgsize) == -1) {
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
            while ((w_id = getNextZero(completed, w)) != -1) {
                if (poll(pfd1s, (nfds_t) w, -1) < 0 && errno != EINTR) {
                    perror("poll");
                    return EC_PIPE;
                }
                while (w_id < w) {
                    if ((pfd1s[w_id].revents & POLLIN) && completed[w_id] == 0) {     // we can read from w_id
                        if (read(pfd1s[w_id].fd, &msgsize, sizeof(size_t)) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        readbuf = realloc(readbuf, msgsize);
                        if (readbuf == NULL) {
                            perror("realloc");
                            return EC_MEM;
                        }
                        if (read(pfd1s[w_id].fd, readbuf, msgsize) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        readbufptr = readbuf;
                        completed[w_id] = 1;
                        readbuf = strtok(readbuf, " ");
                        worker_tf = atoi(readbuf);
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
                        readbuf = readbufptr;
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
                if (write(fd0s[w_id], &msgsize, sizeof(size_t)) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
                if (write(fd0s[w_id], writebuf, msgsize) == -1) {
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
            while ((w_id = getNextZero(completed, w)) != -1) {
                if (poll(pfd1s, (nfds_t) w, -1) < 0 && errno != EINTR) {
                    perror("poll");
                    return EC_PIPE;
                }
                while (w_id < w) {
                    if ((pfd1s[w_id].revents & POLLIN) && completed[w_id] == 0) {     // we can read from w_id
                        if (read(pfd1s[w_id].fd, &msgsize, sizeof(size_t)) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        readbuf = realloc(readbuf, msgsize);
                        if (readbuf == NULL) {
                            perror("realloc");
                            return EC_MEM;
                        }
                        if (read(pfd1s[w_id].fd, readbuf, msgsize) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        readbufptr = readbuf;
                        completed[w_id] = 1;
                        readbuf = strtok(readbuf, " ");
                        worker_tf = atoi(readbuf);
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
                        readbuf = readbufptr;
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
                if (write(fd0s[w_id], &msgsize, sizeof(size_t)) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
                if (write(fd0s[w_id], writebuf, msgsize) == -1) {
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
            while ((w_id = getNextZero(completed, w)) != -1) {
                if (poll(pfd1s, (nfds_t) w, -1) < 0 && errno != EINTR) {
                    perror("poll");
                    return EC_PIPE;
                }
                while (w_id < w) {
                    if ((pfd1s[w_id].revents & POLLIN) && completed[w_id] == 0) {     // we can read from w_id
                        if (read(pfd1s[w_id].fd, &msgsize, sizeof(size_t)) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        readbuf = realloc(readbuf, msgsize);
                        if (readbuf == NULL) {
                            perror("realloc");
                            return EC_MEM;
                        }
                        if (read(pfd1s[w_id].fd, readbuf, msgsize) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        readbufptr = readbuf;
                        completed[w_id] = 1;
                        total_chars += atoi(strtok(readbuf, " "));
                        total_words += atoi(strtok(NULL, " "));
                        total_lines += atoi(strtok(NULL, " "));
                        readbuf = readbufptr;
                    }
                    w_id++;
                }
            }
            printf("Total bytes: %d\n", total_chars);
            printf("Total words: %d\n", total_words);
            printf("Total lines: %d\n", total_lines);
        } else if (!strcmp(command, cmds[4])) {
            printf("jobExecutor: %d\n", getpid());
            for (int w_id = 0; w_id < w; w_id++) {
                printf("Worker%d: %d\n", w_id, pids[w_id]);
            }
        } else if (!strcmp(command, cmds[5])) {
            printf("Available commands (use without quotes):\n");
            printf(" '/search word1 word2 ... -d deadline' for a list of the files that include the given words, along with the lines where they appear.\n");
            printf("    Results will be printed within the seconds given as an integer (deadline).\n");
            printf("    To wait for all the results without a deadline use '-d 0'.\n");
            printf(" '/maxcount word' for the file where the given word appears the most.\n");
            printf(" '/mincount word' for the file where the given word appears the least (but at least once).\n");
            printf(" '/wc' for the number of characters (bytes), words and lines of every file.\n");
            printf(" '/pids' for a list containing jobExecutor's and all its workers' process ids.\n");
            printf(" '/help' for the list you're seeing right now.\n");
            printf(" '/exit' to terminate this program.\n");
            printf(" '/exit -l' to terminate this program and also delete all log files.\n");
        } else if (!strcmp(command, cmds[6])) {       // exit
            signal(SIGCHLD, NULL);      // don't want to invoke SIGCHLD handler at this point
            command = strtok(NULL, " \t");
            for (int w_id = 0; w_id < w; w_id++) {
                if (write(fd0s[w_id], &msgsize, sizeof(size_t)) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
                if (write(fd0s[w_id], writebuf, msgsize) == -1) {
                    perror("Error writing to pipe");
                    return EC_PIPE;
                }
            }
            // Get and print "search string found" from each worker:
            int completed[w];
            int w_id;
            for (w_id = 0; w_id < w; w_id++) {
                completed[w_id] = 0;
            }
            int strings_found;
            while ((w_id = getNextZero(completed, w)) != -1) {
                if (poll(pfd1s, (nfds_t) w, -1) < 0 && errno != EINTR) {
                    perror("poll");
                    return EC_PIPE;
                }
                while (w_id < w) {
                    if ((pfd1s[w_id].revents & POLLIN) && completed[w_id] == 0) {     // we can read from w_id
                        if (read(pfd1s[w_id].fd, &strings_found, sizeof(int)) < 0) {
                            perror("Error reading from pipe");
                            return EC_PIPE;
                        }
                        completed[w_id] = 1;
                        printf("Worker%d found %d search strings.\n", w_id, strings_found);
                    }
                    w_id++;
                }
            }
            // We don't want any zombies:
            for (w_id = 0; w_id < w; w_id++) {
                waitpid(pids[w_id], NULL, 0);
            }
            // Deleting logs, if "-l" was specified:
            if (command != NULL && !strcmp(command, "-l")) {
                int del_error = 0;
                DIR *dirp;
                struct dirent *curr_dirent;
                char curr_dirname[PATH_MAX + 1], curr_filename[PATH_MAX + 1];
                for(w_id = 0; w_id < w; w_id++) {
                    sprintf(curr_dirname, "%s/Worker%d", LOGPATH, w_id);
                    if ((dirp = opendir(curr_dirname)) == NULL) {
                        perror("Error opening log directory for deletion");
                        del_error = 1;
                        continue;
                    }
                    while ((curr_dirent = readdir(dirp))) {
                        if ((strcmp(curr_dirent->d_name, ".") != 0) && (strcmp(curr_dirent->d_name, "..") != 0)) {
                            sprintf(curr_filename, "%s/%s", curr_dirname, curr_dirent->d_name);
                            if (unlink(curr_filename) < 0) {
                                perror("Error deleting logfile");
                                del_error = 1;
                            }
                        }
                    }
                    closedir(dirp);
                    if (rmdir(curr_dirname) < 0) {
                        perror("Error deleting log directory");
                        del_error = 1;
                    }
                }
                if (rmdir(LOGPATH) < 0) {
                    perror("Error deleting log directory");
                    del_error = 1;
                }
                if (del_error) {
                    printf("Log files were not deleted due to an unexpected error.\n");
                } else {
                    printf("Log files were successfully deleted.\n");
                }
            }
            break;
        } else {
            fprintf(stderr, "Unknown command '%s': Type '/help' for a detailed list of available commands.\n", command);
        }
        buffer = bufferptr;
    }
    if (executorKilled) {
        fprintf(stderr, "jobExecutor was killed\n");
        signal(SIGCHLD, NULL);      // don't want to invoke SIGCHLD handler at this point
        for (int w_id = 0; w_id < w; w_id++) {
            kill(pids[w_id], SIGTERM);
        }
        for (int w_id = 0; w_id < w; w_id++) {
            waitpid(pids[w_id], NULL, 0);
        }
    }

    // Free everything:
    for (int w_id = 0; w_id < w; w_id++) {
        if (close(fd0s[w_id]) < 0 || close(pfd1s[w_id].fd) < 0) {
            perror("Error closing pipes");
        }
        sprintf(fifo0_name, "%s/Worker%d_0", PIPEPATH, w_id);
        sprintf(fifo1_name, "%s/Worker%d_1", PIPEPATH, w_id);
        if (unlink(fifo0_name) < 0 || unlink(fifo1_name) < 0) {
            perror("Error deleting pipes");
        }
    }
    if (rmdir(PIPEPATH) < 0) {
        perror("rmdir");
    }
    if (bufferptr != NULL) {
        free(bufferptr);
    }
    if (writebuf != NULL) {
        free(writebuf);
    }
    if (readbuf != NULL) {
        free(readbuf);
    }
    for (int i = 0; i < dirs_num; i++) {
        free(dirnames[i]);
    }
    printf("jobExecutor has exited.\n");
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

void child_handler(int signum) {   // Used when a worker is terminated to recreate it
    pid_t terminated_worker_pid = wait(NULL);
    if (terminated_worker_pid != 0) {
        int w_id = 0;
        for (w_id = 0; w_id < w; w_id++) {      // find terminated worker's w_id
            if (pidsptr[w_id] == terminated_worker_pid) {
                child_aliveptr[w_id] = 0;
                break;
            }
        }
    }
}
