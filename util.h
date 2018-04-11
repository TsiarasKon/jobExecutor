#ifndef UTIL_H
#define UTIL_H

enum ErrorCodes {EC_OK,     // Success
    EC_ARG,      // Invalid command line arguments
    EC_DIR,      // Failed to open/create directory
    EC_FILE,     // Failed to open/create text file
    EC_FORK,     // Error while forking
    EC_PIPE,     // Error related to pipes
    EC_CMD,      // Failed to run external command
    EC_MEM,      // Failed to allocate memory
    EC_UNKNOWN   // An unexpected error
};

const char *cmds[6];

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
void destroyStringList(StringList **list);


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
void destroyIntList(IntList **list);


int getArrayMax(const int arr[], int dim);
char* getCurrentTime(void);

#endif
