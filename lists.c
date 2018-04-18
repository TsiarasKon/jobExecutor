#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lists.h"
#include "util.h"

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

int existsInStringList(StringList *list, char *string) {
    if (list != NULL) {
        StringListNode *current = list->first;
        while (current != NULL) {
            if (!strcmp(current->string, string)) {
                return 1;
            }
            current = current->next;
        }
    }
    return 0;
}

void deleteStringList(StringList **list) {
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
    free(*list);
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

int existsInIntList(IntList *list, int x) {
    if (list != NULL) {
        IntListNode *current = list->first;
        while (current != NULL) {
            if (current->line == x) {
                return 1;
            }
            current = current->next;
        }
    }
    return 0;
}

void deleteIntList(IntList **list) {
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
    free(*list);
    *list = NULL;
}