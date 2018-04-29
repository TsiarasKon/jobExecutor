#ifndef POSTINGLIST_H
#define POSTINGLIST_H

#include "trie.h"
#include "lists.h"

#ifndef TYPEDEFS     // needed forward declarations, avoiding redefinitions
#define TYPEDEFS
typedef struct postinglist PostingList;
typedef struct trienode TrieNode;
#endif
typedef struct postinglistnode PostingListNode;

struct postinglistnode {
    int id;
    IntList *lines;
    int tf;
    PostingListNode *next;
};

struct postinglist {
    PostingListNode *first;
    PostingListNode *last;
};

PostingListNode *createPostingListNode(int id, int line);
void deletePostingListNode(PostingListNode **listNode);
PostingList* createPostingList();
void deletePostingList(PostingList **postingList);

int incrementPostingList(TrieNode *node, int id, int line);

#endif
