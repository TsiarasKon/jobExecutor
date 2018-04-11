#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "util.h"

const char *cmds[6] = {
        "/search",
        "/maxcount",
        "/mincount",
        "/wc",
        "/help",
        "/exit"
};

StringList* createStringList() {
    StringList *list = malloc(sizeof(StringList));
    if (list == NULL) {
        perror("malloc");
        return NULL;
    }
    list->first = list->last = NULL;
    return list;
}

StringListNode* createStringListNode(char *string) {
    StringListNode *listNode = malloc(sizeof(StringListNode));
    if (listNode == NULL) {
        perror("malloc");
        return NULL;
    }
    listNode->string = malloc(strlen(string) + 1);
    if (listNode->string == NULL) {
        perror("malloc");
        return NULL;
    }
    strcpy(listNode->string, string);
    listNode->next = NULL;
    return listNode;
}

int appendStringListNode(StringList *list, char *string) {
    if (list->first == NULL) {
        list->first = createStringListNode(string);
        if (list->first == NULL) {
            return EC_MEM;
        }
        list->last = list->first;
        return EC_OK;
    }
    list->last->next = createStringListNode(string);
    if (list->last->next == NULL) {
        return EC_MEM;
    }
    list->last = list->last->next;
    return EC_OK;
}

void destroyStringList(StringList **list) {
    if (*list == NULL) {
        fprintf(stderr, "Attempted to delete a NULL StringList.\n");
        return;
    }
    StringListNode *current = (*list)->first;
    StringListNode *next;
    while (current != NULL) {
        next = current->next;
        free(current->string);
        free(current);
        current = next;
    }
    *list = NULL;
}


IntList* createIntList() {
    IntList *list = malloc(sizeof(IntList));
    if (list == NULL) {
        perror("malloc");
        return NULL;
    }
    list->first = list->last = NULL;
    return list;
}

IntListNode* createIntListNode(int x) {
    IntListNode *listNode = malloc(sizeof(IntListNode));
    if (listNode == NULL) {
        perror("malloc");
        return NULL;
    }
    listNode->line = x;
    listNode->next = NULL;
    return listNode;
}

int appendIntListNode(IntList *list, int x) {
    if (list->first == NULL) {
        list->first = createIntListNode(x);
        if (list->first == NULL) {
            return EC_MEM;
        }
        list->last = list->first;
        return EC_OK;
    }
    list->last->next = createIntListNode(x);
    if (list->last->next == NULL) {
        return EC_MEM;
    }
    list->last = list->last->next;
    return EC_OK;
}

void destroyIntList(IntList **list) {
    if (*list == NULL) {
        fprintf(stderr, "Attempted to delete a NULL StringList.\n");
        return;
    }
    IntListNode *current = (*list)->first;
    IntListNode *next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    *list = NULL;
}

int getArrayMax(const int arr[], int dim) {
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


char* getCurrentTime(void) {
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    static char output[20];
    sprintf(output, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return output;
}


