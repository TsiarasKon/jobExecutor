#ifndef JOBEXECUTOR_LISTS_H
#define JOBEXECUTOR_LISTS_H

/* The following 2 lists (string and int) are basically identical
 * In another language I would have used templates but in C
 * I opted to avoid void pointers to simulate that behaviour. */
typedef struct stringlistnode StringListNode;
struct stringlistnode {
    char *string;
    StringListNode *next;
};
StringListNode* createStringListNode(char *string);

typedef struct stringlist StringList;
struct stringlist {
    StringListNode *first;
    StringListNode *last;
};
StringList* createStringList();
int appendStringListNode(StringList *list, char *string);
int existsInStringList(StringList *list, char *string);
void deleteStringList(StringList **list);


typedef struct intlistnode IntListNode;
struct intlistnode {
    int line;
    IntListNode *next;
};
IntListNode* createIntListNode(int x);

typedef struct intlist IntList;
struct intlist {
    IntListNode *first;
    IntListNode *last;
};
IntList* createIntList();
int appendIntListNode(IntList *list, int x);
int existsInIntList(IntList *list, int x);
void deleteIntList(IntList **list);

#endif
