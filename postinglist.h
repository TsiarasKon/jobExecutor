#ifndef POSTINGLIST_H
#define POSTINGLIST_H

#include "trie.h"

#ifndef TYPEDEFS     // needed forward declarations, avoiding redefinitions
#define TYPEDEFS
typedef struct postinglist PostingList;
typedef struct trienode TrieNode;
#endif
typedef struct listnode ListNode;

struct listnode {
    int id_times[2];
    ListNode *next;
};

struct postinglist {
    int df;
    ListNode *first;
    ListNode *last;
};

ListNode* createListNode(int id);
void deleteListNode(ListNode **listNode);
PostingList* createPostingList();
void deletePostingList(PostingList **postingList);

int incrementPostingList(TrieNode *node, int id);
int getTermFrequency(PostingList *postingList, int id);

#endif
