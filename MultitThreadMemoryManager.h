#ifndef MTMM_H
#define MTMM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "Init.h"

#define ThreadUnit DataSizeType
#define MemorySize DataSizeType
#define UnitSize DataSizeType

typedef struct mi {
    struct mi *back, *next;
    size_t unit;
    int isfree;
    void *m;
    MemorySize size;
    pthread_t id;
} MemoryInfo;

typedef struct mbu {
    struct msu *snext, *msuw;
    pthread_t id;
    size_t unit;
} MemoryBigUnit;

typedef struct msu {
    struct msu *next;
    void *m;
    MemoryInfo *minfo, *minfow;
    MemorySize size, length, fsize;
    size_t unit;
} MemorySmallUnit;

typedef struct mbul {
    struct mbul *bnext;
    MemoryBigUnit **mbu;
    size_t length, zero;
} MemoryBigUnitList;

typedef struct mm {
    MemoryBigUnitList *mbu;
    MemoryBigUnitList *mbuw;
    UnitSize msusize, listsize;
} MTMemoryManager;

#define MemoryBigUnitSize sizeof(MemoryBigUnit)
#define MemorySmallUnitSize sizeof(MemorySmallUnit)
#define MemoryInfoSize sizeof(MemoryInfo)

MTMemoryManager *MTMemoryManagerInit(ThreadUnit Length, UnitSize us);

MemoryBigUnit *NewBigUnit(MTMemoryManager *mm);

MemorySmallUnit *NewSmallUnit(MemoryBigUnit *mbu, UnitSize us);

MemoryInfo *UnitMalloc(MemorySmallUnit *mu, MemorySize s);

void UnitFree(MemorySmallUnit *mu, MemoryInfo *m);

MemoryInfo *SmallUnitAlloc(MTMemoryManager *mm,MemoryBigUnit *mbu, MemorySmallUnit *msu, MemorySize ms);

MemoryInfo *MTMemoryManagerAlloc(MTMemoryManager *mm, MemorySize ms);

MemoryInfo *MTMemoryManagerCalloc(MTMemoryManager *mm, MemorySize ms);

MemoryInfo *MTMemoryManagerRealloc(MTMemoryManager *mm, MemoryInfo *mi, MemorySize ms);

void MTMemoryManagerFree(MTMemoryManager *mm, MemoryInfo *mi);

MemoryInfo *MTMemoryManagerUnitAlloc(MTMemoryManager *mm, MemoryBigUnit *mbu, MemorySize ms);

MemoryInfo *MTMemoryManagerUnitCalloc(MTMemoryManager *mm, MemoryBigUnit *mbu, MemorySize ms);

void MTMemoryManagerUnitFree(MemoryBigUnit *mbu, MemoryInfo *mi);

MemoryInfo *MTMemoryManagerUnitRealloc(MTMemoryManager *mm, MemoryBigUnit *mu, MemoryInfo *mi, MemorySize ms);

MemoryBigUnit *MTMemoryManagerBindingThread(MTMemoryManager *mm, pthread_t id);

MemoryBigUnit *MTMemoryManagerCreateUnitAndBindingThread(MTMemoryManager *mm, pthread_t id);

void MTMemoryManagerManualBindingThread(MTMemoryManager *mm, MemoryBigUnit *mbu, pthread_t id);

void MTMemoryManagerCompleteInitUnit(MTMemoryManager *mm, pthread_t id);

void MTMemoryManagerAppointComleteInitUnit(MemoryBigUnit *mbu);

void MTMemoryManagerDestroy(MTMemoryManager *mm);

void MemoryUnitCheck(MTMemoryManager *mm);

void MTMemoryManagerManualCleanUnit(MemoryBigUnit *mbu);

void MTMemoryManagerManualCleanThread(MemoryBigUnit *mbu);

void MTMemoryManagerCleanUnit(MTMemoryManager *mm, pthread_t id);

bool MTMemoryManagerCMP(MemoryInfo *mi, MemoryInfo *mk, MemorySize s);

void MTMemoryManagerCleanThread(MTMemoryManager *mm, pthread_t id);

void MTMemoryManagerInitUnit(MTMemoryManager *mm, pthread_t id);

#endif
