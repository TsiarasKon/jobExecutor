#ifndef POSTINGLIST_H
#define POSTINGLIST_H

#include "trie.h"
#include "util.h"

#ifndef TYPEDEFS     // needed forward declarations, avoiding redefinitions
#define TYPEDEFS
typedef struct postinglist PostingList;
typedef struct trienode TrieNode;
#endif
typedef struct postinglistnode PostingListNode;

struct postinglistnode {
    int id;
    IntListNode *firstline;
    IntListNode *lastline;
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
int getTermFrequency(PostingList *postingList, int id);

#endif
