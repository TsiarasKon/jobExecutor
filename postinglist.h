#ifndef POSTINGLIST_H
#define POSTINGLIST_H

#include "trie.h"

#ifndef TYPEDEFS     // needed forward declarations, avoiding redefinitions
#define TYPEDEFS
typedef struct postinglist PostingList;
typedef struct trienode TrieNode;
#endif
typedef struct postinglistnode PostingListNode;
typedef struct linelistnode LineListNode;

struct linelistnode {
    int line;
    LineListNode *next;
};

struct postinglistnode {
    int id;
    char *filename;
    LineListNode *firstline;
    LineListNode *lastline;
    int tf;
    PostingListNode *next;
};

struct postinglist {
    PostingListNode *first;
    PostingListNode *last;
};

LineListNode* createLineListNode(int line);

PostingListNode* createPostingListNode(int id, char *filename, int line);
void deletePostingListNode(PostingListNode **listNode);
PostingList* createPostingList();
void deletePostingList(PostingList **postingList);

int incrementPostingList(TrieNode *node, int id, char *filename, int line);
int getTermFrequency(PostingList *postingList, int id);

#endif
