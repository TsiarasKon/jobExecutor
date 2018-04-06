#ifndef TRIE_H
#define TRIE_H

#include "postinglist.h"

#ifndef TYPEDEFS     // needed forward declarations, avoiding redefinitions
#define TYPEDEFS
typedef struct postinglist PostingList;
typedef struct trienode TrieNode;
#endif
typedef struct trie Trie;

struct trienode {
    char value;
    TrieNode *next;
    TrieNode *child;
    PostingList *postingList;
};

struct trie {
    TrieNode *first;
};

TrieNode* createTrieNode(char value, TrieNode *next);
void deleteTrieNode(TrieNode **trieNode);
Trie* createTrie();
void deleteTrie(Trie **trie);

int directInsert(TrieNode *current, char *word, int id, char *filename, int line, int i);
int insert(Trie *root, char *word, int id, char *filename, int line);

PostingList *getPostingList(Trie *root, char *word);

//int printTrieNode(TrieNode *node, char *prefix);
//int printTrie(Trie *root);

#endif
