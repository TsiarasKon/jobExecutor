#ifndef UTIL_H
#define UTIL_H

enum ErrorCodes {EC_OK, EC_ARG, EC_DIR, EC_FILE, EC_FORK, EC_PIPE, EC_CMD, EC_MEM, EC_UNKNOWN};
/* Error codes:
 * EC_OK:      Success
 * EC_ARG:     Invalid command line arguments
 * EC_DIR:     Failed to open/create directory
 * EC_FILE:    Failed to open/create text file
 * EC_FORK:    Error while forking
 * EC_PIPE:    Error related to named pipes
 * EC_CMD:     Failed to run external command
 * EC_MEM:     Failed to allocate memory
 * EC_UNKNOWN: An unexpected error
*/

const char *cmds[6];

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

int getArrayMax(const int arr[], int dim);
char* getCurrentTime(void);

#endif
