#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "util.h"

const char *cmds[7] = {
        "/search",
        "/maxcount",
        "/mincount",
        "/wc",
        "/pids",
        "/help",
        "/exit"
};

int getArrayMax(const int *arr, int dim) {
    if (dim == 0) {
        return -1;
    }
    int curr_max = arr[0];
    for (int i = 1; i < dim; i++) {
        if (arr[i] > curr_max) {
            curr_max = arr[i];
        }
    }
    return curr_max;
}

int getNextZero(const int *arr, int dim) {
    for (int i = 0; i < dim; i++) {
        if (arr[i] == 0) {
            return i;
        }
    }
    return -1;
}


char* getCurrentTime(void) {
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    static char output[20];
    sprintf(output, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return output;
}
