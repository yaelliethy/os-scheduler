#include <stdlib.h>
#include "page_table.h"
PageTable* createPageTable(int limit)
{
    if (limit <= 0) return NULL;
    PageTable *pt = (PageTable *)malloc(sizeof(PageTable));
    if (pt == NULL) return NULL;
    pt->limit = limit;
    pt->entries = (PTE *)malloc(limit * sizeof(PTE));
    if (pt->entries == NULL)
    {
        free(pt);
        return NULL;
    }
    for (int i = 0; i < limit; i++)
    {
        pt->entries[i].frameNumber = -1;
        pt->entries[i].valid = 0;
        pt->entries[i].R = 0;
        pt->entries[i].M = 0;
    }
    return pt;
}

void freePageTable(PageTable *pt)
{
    if (pt == NULL) return;
    free(pt->entries);
    free(pt);
}

int getPageNum(int VA)
{
    if (VA < 0) return -1;
    return VA / PAGE_SIZE;
}

int getOffset(int VA)
{
    if (VA < 0) return -1;
    return VA % PAGE_SIZE;
}

int isPageValid(PageTable *pt, int pageNum)
{
    if (pt ==NULL) return 0;
    if (pageNum < 0 || pageNum >= pt->limit) return 0;
    return pt->entries[pageNum].valid;
}

int translateAddress(PageTable *pt, int VA)
{
    if (pt ==NULL) return -1;
    int pageNum = getPageNum(VA);
    int offset = getOffset(VA);
    if (pageNum< 0 || offset <0) return -1;
    if (!isPageValid(pt, pageNum)) return -1;
    int frameNum = pt->entries[pageNum].frameNumber;
    return frameNum * PAGE_SIZE + offset;
}

void updateBits(PageTable *pt, int pageNum, char op)
{
    if (pt == NULL) return;
    if (pageNum < 0 || pageNum >= pt->limit) return;
    if (pt->entries[pageNum].valid == 0) return;
    pt->entries[pageNum].R = 1;
    if (op == 'w' || op == 'W') pt->entries[pageNum].M = 1;
}

void setPageEntry(PageTable *pt, int pageNum, int frameNum)
{
    if (pt == NULL) return;
    if (pageNum < 0 || pageNum >= pt->limit) return;
    pt->entries[pageNum].frameNumber = frameNum;
    pt->entries[pageNum].valid = 1;
    pt->entries[pageNum].R = 0;
    pt->entries[pageNum].M = 0;
}

void invalidatePage(PageTable *pt, int pageNum)
{
    if (pt == NULL) return;
    if (pageNum < 0 || pageNum >= pt->limit) return;
    pt->entries[pageNum].frameNumber = -1;
    pt->entries[pageNum].valid = 0;
    pt->entries[pageNum].R = 0;
    pt->entries[pageNum].M = 0;
}