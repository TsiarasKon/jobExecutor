#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

IntListNode* createIntListNode(int x) {
    IntListNode *listNode = malloc(sizeof(IntListNode));
    if (listNode == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return NULL;
    }
    listNode->line = x;
    listNode->next = NULL;
    return listNode;
}

StringListNode* createStringListNode(char *string) {
    StringListNode *listNode = malloc(sizeof(StringListNode));
    if (listNode == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return NULL;
    }
    listNode->string = malloc(strlen(string) + 1);
    if (listNode->string == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return NULL;
    }
    strcpy(listNode->string, string);
    listNode->next = NULL;
    return listNode;
}

void deleteStringList(StringListNode **head) {
    StringListNode **current = head;
    StringListNode **next;
    while (*current != NULL) {
        next = &(*current)->next;
        free((*current)->string);
        free(current);
        current = next;
    }
    *head = NULL;
}