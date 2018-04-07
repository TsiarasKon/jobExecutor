#ifndef UTIL_H
#define UTIL_H


typedef struct stringlistnode StringListNode;
struct stringlistnode {
    char *string;
    StringListNode *next;
};
StringListNode* createStringListNode(char *string);

typedef struct intlistnode IntListNode;
struct intlistnode {
    int line;
    IntListNode *next;
};
IntListNode* createIntListNode(int x);


#endif
