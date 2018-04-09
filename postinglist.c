#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "postinglist.h"

PostingListNode *createPostingListNode(int id, int line) {
    PostingListNode *listNode = malloc(sizeof(PostingListNode));
    if (listNode == NULL) {
        perror("malloc");
        return NULL;
    }
    listNode->id = id;
    listNode->firstline = createIntListNode(line);
    if (listNode->firstline == NULL) {
        perror("malloc");
        return NULL;
    }
    listNode->lastline = listNode->firstline;
    listNode->tf = 1;
    listNode->next = NULL;
    return listNode;
}

void deletePostingListNode(PostingListNode **listNode) {
    if (*listNode == NULL) {
        fprintf(stderr, "Attempted to delete a NULL PostingListNode.\n");
        return;
    }
    PostingListNode *current = *listNode;
    PostingListNode *next;
    IntListNode *currentlinenode, *nextlinenode;
    while (current != NULL) {
        currentlinenode = current->firstline;
        while (currentlinenode != NULL) {       // Also delete LineList
            nextlinenode = currentlinenode->next;
            free(currentlinenode);
            currentlinenode = nextlinenode;
        }
        next = current->next;
        free(current);
        current = next;
    }
    *listNode = NULL;
}

PostingList* createPostingList() {
    PostingList *postingList = malloc(sizeof(PostingList));
    if (postingList == NULL) {
        perror("malloc");
        return NULL;
    }
    postingList->first = postingList->last = NULL;
    return postingList;
}

void deletePostingList(PostingList **postingList) {
    if (*postingList == NULL) {
        fprintf(stderr, "Attempted to delete a NULL PostingList.\n");
        return;
    }
    if ((*postingList)->first != NULL) {
        deletePostingListNode(&(*postingList)->first);     // delete the entire list
    }
    free(*postingList);
    *postingList = NULL;
}

int incrementPostingList(TrieNode *node, int id, int line) {
    PostingList **PostingList = &node->postingList;
    // If list is empty, create a listNode and set both first and last to point to it
    if ((*PostingList)->first == NULL) {
        (*PostingList)->first = createPostingListNode(id, line);
        if ((*PostingList)->first == NULL) {
            perror("Error allocating memory");
            return EC_MEM;
        }
        (*PostingList)->last = (*PostingList)->first;
        return EC_OK;
    }
    /* Words are inserted in order of id, so the posting list we're looking for either
     * is the last one or it doesn't exist and should be created after the last */
    if ((*PostingList)->last->id == id) {      // word belongs to last doc
        (*PostingList)->last->lastline->next = createIntListNode(line);    // append a IntListNode
        if ((*PostingList)->last->lastline->next == NULL) {
            perror("Error allocating memory");
            return EC_MEM;
        }
        (*PostingList)->last->lastline = (*PostingList)->last->lastline->next;  // update pointer to lastline
        (*PostingList)->last->tf++;
    } else {
        (*PostingList)->last->next = createPostingListNode(id, line);
        if ((*PostingList)->last->next == NULL) {
            perror("Error allocating memory");
            return EC_MEM;
        }
        (*PostingList)->last = (*PostingList)->last->next;
    }
    return EC_OK;
}

int getTermFrequency(PostingList *postingList, int id) {        // returns 0 if not found
    if (postingList == NULL) {
        return 0;
    }
    PostingListNode *current = postingList->first;
    // If we surpass the id, then the postingList we're searching for doesn't exist:
    while (current != NULL && current->id <= id) {
        if (current->id == id) {
            return current->tf;
        }
        current = current->next;
    }
    return 0;
}
