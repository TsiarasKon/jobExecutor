#ifndef DOC_STRUCT_H
#define DOC_STRUCT_H

#define PIPEPATH "./pipes"
#define LOGPATH "./log"

typedef struct docs Docs;

struct docs {
    char *path;
    char **lines;
    Docs *next;
};


#endif
