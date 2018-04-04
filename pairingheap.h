#ifndef PAIRINGHEAP_H
#define PAIRINGHEAP_H

typedef struct heapNode HeapNode;

struct heapNode {
    double score;
    int id;
    HeapNode *sibilings;
    HeapNode *children;
};

HeapNode* createHeapNode(double score, int id);
void destroyHeap(HeapNode **heap);
void addHeapChild(HeapNode *heap, HeapNode *heapNode);

HeapNode* heapMerge(HeapNode *heap1, HeapNode *heap2);
HeapNode* mergePairs(HeapNode *children);
HeapNode* heapInsert(HeapNode *heap, double score, int id);
HeapNode* deleteMaxNode(HeapNode **heap);

int getHeapSize(HeapNode *heap);

#endif
