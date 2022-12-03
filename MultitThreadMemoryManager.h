#ifndef MTMM_H
#define MTMM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#define ThreadUnit size_t
#define MemorySize size_t
#define UnitSize size_t

typedef struct mi {
    struct mi *back, *next;
    size_t unit, isfree, freelistindex, size;
    void *m;
    pthread_t id;
} MemoryInfo;

typedef struct msu {
    void *m;
    MemoryInfo *infohead, *infotail;
    MemoryInfo **freelist;
    size_t *freelistmake;
    size_t freelistmakesize, freelistsize, standard, size, length, freesize, unit, msuindex;
} MemorySmallUnit;

typedef struct mbu {
    pthread_t id;
    size_t unit, msulistsize, unitsizestandard, msuliststandard, msulistlength;
    MemorySmallUnit **msulist;
} MemoryBigUnit;

typedef struct mm {
    size_t mbuliststandard, smallunitsize, mbulistlength;
    MemoryBigUnit **mbulist;
} MTMemoryManager;

#define MemoryBigUnitSize sizeof(MemoryBigUnit)
#define MemorySmallUnitSize sizeof(MemorySmallUnit)
#define MemoryInfoSize sizeof(MemoryInfo)

MTMemoryManager *MTMemoryManagerInit(UnitSize us);

MemoryBigUnit *NewBigUnit(MTMemoryManager *mm);

MemorySmallUnit *NewSmallUnit(MemoryBigUnit *mbu, UnitSize us);

MemoryInfo *UnitMalloc(MemorySmallUnit *mu, MemorySize s);

int UnitFree(MemorySmallUnit *mu, MemoryInfo *m);

MemoryInfo *SmallUnitAlloc(MemoryBigUnit *mbu, MemorySize ms);

MemoryInfo *MTMemoryManagerAlloc(MTMemoryManager *mm, MemorySize ms);

MemoryInfo *MTMemoryManagerCalloc(MTMemoryManager *mm, MemorySize ms);

MemoryInfo *MTMemoryManagerRealloc(MTMemoryManager *mm, MemoryInfo *mi, MemorySize ms);

int MTMemoryManagerFree(MTMemoryManager *mm, MemoryInfo *mi);

MemoryInfo *MTMemoryManagerUnitAlloc(MemoryBigUnit *mbu, MemorySize ms);

MemoryInfo *MTMemoryManagerUnitCalloc(MemoryBigUnit *mbu, MemorySize ms);

int MTMemoryManagerUnitFree(MemoryBigUnit *mbu, MemoryInfo *mi);

MemoryInfo *MTMemoryManagerUnitRealloc(MemoryBigUnit *mbu, MemoryInfo *mi, MemorySize ms);

MemoryBigUnit *MTMemoryManagerCreateUnitAndBindingThread(MTMemoryManager *mm, pthread_t id);

MemoryBigUnit *MTMemoryManagerBindingThread(MTMemoryManager *mm, pthread_t id);

void MTMemoryManagerForceBindingThread(MemoryBigUnit *mbu, pthread_t id);

void MTMemoryManagerDestroy(MTMemoryManager *mm);

void MTMemoryManagerManualCleanUnit(MemoryBigUnit *mbu);

void MTMemoryManagerManualCleanThread(MemoryBigUnit *mbu);

int MTMemoryManagerCleanUnit(MTMemoryManager *mm, pthread_t id);

void MTMemoryManagerAppointInitUnit(MemoryBigUnit *mbu);

int MTMemoryManagerInitUnit(MTMemoryManager *mm, pthread_t id);

bool MTMemoryManagerCMP(MemoryInfo *mi, MemoryInfo *mk, MemorySize s);

void MemoryUnitCheck(MTMemoryManager *mm);

#endif
