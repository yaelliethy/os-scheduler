#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H
#define PAGE_SIZE 16
typedef struct
{
    int frameNumber;
    int valid;
    int R;
    int M;
} PTE;
typedef struct
{
    PTE *entries;
    int limit;

} PageTable;

PageTable* createPageTable(int limit);
void freePageTable(PageTable *pt);
int getPageNum(int VA);
int getOffset(int VA);
int isPageValid(PageTable *pt, int pageNum);
int translateAddress(PageTable *pt, int VA);
void updateBits(PageTable *pt, int pageNum, char op);
void setPageEntry(PageTable *pt, int pageNum, int frameNum);
void invalidatePage(PageTable *pt, int pageNum);
#endif