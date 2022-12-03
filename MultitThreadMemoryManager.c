#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include "MultitThreadMemoryManager.h"

MTMemoryManager *MTMemoryManagerInit(UnitSize us) {
    MTMemoryManager *mm = sbrk(0);
    brk(mm + sizeof(MTMemoryManager));
    mm->mbulist = sbrk(0);
    brk(mm->mbulist + (sizeof(MemoryBigUnit *) << 2));
    mm->mbuliststandard = 2;
    mm->smallunitsize = us;
    mm->mbulistlength = 0;
    return mm;
}

MemoryBigUnit *NewBigUnit(MTMemoryManager *mm) {
    MemoryBigUnit *mbu = sbrk(0);
    brk(mbu + MemoryBigUnitSize);
    mbu->id = 0;
    mbu->unit = mm->mbulistlength;
    mbu->unitsizestandard = mm->smallunitsize;
    mbu->msulistsize = 4;
    mbu->msulistlength = 0;
    mbu->msuliststandard = 2;
    mbu->msulist = sbrk(0);
    brk(mbu->msulist + (sizeof(MemorySmallUnit *) << 2));
    if ((mm->mbulistlength + 1) > (1 << mm->mbuliststandard)) {
        void *mbulisttmp = sbrk(0);
        brk(mbulisttmp + (sizeof(MemoryBigUnit *) << (mm->mbuliststandard << 1)));
        memcpy(mbulisttmp, mm->mbulist, (sizeof(MemoryBigUnit *) << mm->mbuliststandard));
        mm->mbulist = NULL;
        brk(mm->mbulist);
        mm->mbuliststandard <<= 1;
        mm->mbulist = mbulisttmp;
    }
    mm->mbulist[mm->mbulistlength] = mbu;
    mm->mbulistlength++;
    return mbu;
}

MemorySmallUnit *NewSmallUnit(MemoryBigUnit *mbu, UnitSize us) {
    MemorySmallUnit *msu = sbrk(0);
    if (us != 0) {
        brk(msu + (MemorySmallUnitSize + us));
        msu->size = us;
    } else {
        brk(msu + (MemorySmallUnitSize + mbu->unitsizestandard));
        msu->size = mbu->unitsizestandard;
    }

    msu->freelist = sbrk(0);
    brk(msu->freelist + (sizeof(MemoryInfo *) << 12));

    msu->freelistmake = sbrk(0);
    brk(msu->freelistmake + (sizeof(size_t) << 12));

    msu->m = (msu + MemorySmallUnitSize);

    msu->unit = mbu->msulistlength;
    msu->length = 0;
    msu->freesize = 0;
    msu->freelistmakesize = 0;
    msu->freelistsize = 0;
    msu->standard = 12;
    if ((mbu->msulistlength + 1) > (1 << mbu->msuliststandard)) {
        MemorySmallUnit **msulisttmp = sbrk(0);
        brk(msulisttmp + (sizeof(MemorySmallUnit *) << (mbu->msuliststandard << 1)));
        memcpy(msulisttmp, mbu->msulist, (sizeof(MemorySmallUnit *) << mbu->msuliststandard));
        mbu->msulist = NULL;
        brk(mbu->msulist);
        mbu->msuliststandard <<= 1;
        mbu->msulist = msulisttmp;
    }
    msu->msuindex = mbu->msulistlength;
    mbu->msulist[mbu->msulistlength] = msu;
    mbu->msulistlength++;
    return msu;
}

MemoryInfo *UnitMalloc(MemorySmallUnit *mu, MemorySize s) {
    MemoryInfo *m;
    if (mu->infohead != NULL && mu->freesize >= s + MemoryInfoSize) {
        for (int i = 0; i < mu->freelistsize; ++i) {
            if (mu->freelist[i] == NULL) {
                continue;
            }
            m = mu->freelist[i];
            if (m->size == s) {
                m->isfree = 0;
                //m->freelistindex = -1;
                mu->freesize -= m->size + MemoryInfoSize;
                mu->freelistmake[mu->freelistmakesize] = i;
                mu->freelist[i] = NULL;
                mu->freelistsize = i > mu->freelistsize ? i : mu->freelistsize-1;
                mu->freelistmakesize++;
                return m;
            }
            if (m->size > s + MemoryInfoSize) {
                m->size -= s + MemoryInfoSize;
                mu->freesize -= s + MemoryInfoSize;
                MemoryInfo *mi = (MemoryInfo *) (m->m + m->size);
                mi->m = (m->m + m->size + MemoryInfoSize);
                mi->unit = mu->unit;
                mi->size = s;
                mi->isfree = 0;
                mi->next = m->next;
                mi->back = m;
                //mi->freelistindex = -1;
                if (m->next != NULL) {
                    m->next->back = mi;
                }
                if (mu->infotail == m) {
                    mu->infotail = mi;
                }
                m->next = mi;
                return mi;
            }
        }
    }

    if (mu->size - mu->length >= s + MemoryInfoSize) {
        MemoryInfo *mi = (MemoryInfo *) (mu->m + mu->length);
        mu->length += MemoryInfoSize;
        mi->m = (mu->m + mu->length);
        mu->length += s;
        mi->unit = mu->unit;
        mi->size = s;
        mi->isfree = 0;
        mi->next = NULL;
        mi->back = mu->infotail;
        //mi->freelistindex = -1;
        if (mu->infohead == NULL) {
            mu->infohead = mi;
            mu->infotail = mi;
        } else {
            mu->infotail->next = mi;
            mu->infotail = mi;
        }
        return mi;
    }
    return NULL;
}

int UnitFree(MemorySmallUnit *mu, MemoryInfo *m) {
    if (m->isfree == 0) {
        m->isfree = 1;
        mu->freesize += (m->size + MemoryInfoSize);
        MemoryInfo *mi = m->back;
        if (mi != NULL && mi->isfree /*&& (mi->m + mi->size) == m*/) {
            mi->size += m->size + MemoryInfoSize;
            mi->next = m->next;
            if (m->next != NULL) {
                m->next->back = mi;
            }
            mu->infotail = mu->infotail == m ? mi : mu->infotail;
            mu->infohead = mu->infohead == m ? mi : mu->infohead;
            /*mu->freelist[mi->freelistindex] = NULL;
            mu->freelistmake[mu->freelistmakesize] = mi->freelistindex;
            mu->freelistsize = mi->freelistindex > mu->freelistsize ? mi->freelistindex : mu->freelistsize;
            mu->freelistmakesize++;*/
            if (m->next != NULL && m->next->isfree /*&& (m->m + m->size) == m->next*/) {
                mi->size += m->next->size + MemoryInfoSize;
                mi->next = m->next->next;
                if (m->next->next != NULL) {
                    m->next->next->back = mi;
                }
                mu->infotail = mu->infotail == m->next ? mi : mu->infotail;
                mu->infohead = mu->infohead == m->next ? mi : mu->infohead;
                mu->freelist[m->next->freelistindex] = NULL;
                mu->freelistmake[mu->freelistmakesize] = m->next->freelistindex;
                mu->freelistsize = m->next->freelistindex > mu->freelistsize ? m->next->freelistindex : mu->freelistsize-1;
                mu->freelistmakesize++;
            }
            /*if (mu->freelistmakesize != 0) {
                mi->freelistindex = mu->freelistmake[mu->freelistmakesize - 1];
                mu->freelist[mu->freelistmake[mu->freelistmakesize - 1]] = mi;
                mu->freelistmakesize--;
                mu->freelistsize++;
            } else {
                if ((mu->freelistsize + 1) > (mu->standard << 1)) {
                    MemoryInfo **freelist = sbrk(0);
                    brk(freelist + (sizeof(MemoryInfo *) << (mu->standard << 1)));
                    memcpy(freelist, mu->freelist, (sizeof(MemoryInfo *) << mu->standard));
                    size_t *freemake = sbrk(0);
                    brk(freemake + (sizeof(size_t) << (mu->standard << 1)));
                    memcpy(freemake, mu->freelistmake, (sizeof(size_t) << mu->standard));

                    mu->freelist = NULL;
                    mu->freelistmake = NULL;
                    brk(mu->freelist);
                    brk(mu->freelistmake);

                    mu->freelist = freelist;
                    mu->freelistmake = freemake;
                }
                mi->freelistindex = mu->freelistsize;
                mu->freelist[mu->freelistsize] = mi;
                mu->freelistsize++;
            }*/
            goto end;
        }

        mi = m->next;
        if (mi != NULL && mi->isfree /*&& (m->m + m->size) == mi*/) {
            m->size += mi->size + MemoryInfoSize;
            m->next = mi->next;
            if (mi->next != NULL) {
                mi->next->back = m;
            }
            mu->infotail = mu->infotail == mi ? m : mu->infotail;
            mu->infohead = mu->infohead == mi ? m : mu->infohead;
            mu->freelist[mi->freelistindex] = NULL;
            mu->freelistmake[mu->freelistmakesize] = mi->freelistindex;
            mu->freelistsize = mi->freelistindex > mu->freelistsize ? mi->freelistindex : mu->freelistsize-1;
            mu->freelistmakesize++;
        }
        if (mu->freelistmakesize != 0) {
            m->freelistindex = mu->freelistmake[mu->freelistmakesize - 1];
            mu->freelist[mu->freelistmake[mu->freelistmakesize - 1]] = m;
            mu->freelistmakesize--;
            mu->freelistsize++;
        } else {
            if ((mu->freelistsize + 1) > (mu->standard << 1)) {
                MemoryInfo **freelist = sbrk(0);
                brk(freelist + (sizeof(MemoryInfo *) << (mu->standard << 1)));
                memcpy(freelist, mu->freelist, (sizeof(MemoryInfo *) << mu->standard));
                size_t *freemake = sbrk(0);
                brk(freemake + (sizeof(size_t) << (mu->standard << 1)));
                memcpy(freemake, mu->freelistmake, (sizeof(size_t) << mu->standard));

                mu->freelist = NULL;
                mu->freelistmake = NULL;
                brk(mu->freelist);
                brk(mu->freelistmake);

                mu->freelist = freelist;
                mu->freelistmake = freemake;
            }
            m->freelistindex = mu->freelistsize;
            mu->freelist[mu->freelistsize] = m;
            mu->freelistsize++;
        }
        end:
        return 1;
    }
    return 0;
}

MemoryInfo *SmallUnitAlloc(MemoryBigUnit *mbu, MemorySize ms) {
    MemorySmallUnit *mu;
    MemoryInfo *mi;
    for (int i = 0; i < mbu->msulistlength; ++i) {
        mu = mbu->msulist[i];
        if (mu->size - mu->length >= ms + MemoryInfoSize || mu->freesize >= ms + MemoryInfoSize) {
            if ((mi = UnitMalloc(mu, ms)) != NULL) {
                return mi;
            }
        }
    }
    return UnitMalloc(NewSmallUnit(mbu, 0), ms);
}

MemoryInfo *MTMemoryManagerAlloc(MTMemoryManager *mm, MemorySize ms) {
    pthread_t id = pthread_self();
    for (int i = 0; i < mm->mbulistlength; ++i) {
        if (mm->mbulist[i]->id == id) {
            MemoryInfo *m = SmallUnitAlloc(mm->mbulist[i], ms);
            m->id = id;
            return m;
        }
    }
    MemoryBigUnit *mbu = NewBigUnit(mm);
    NewSmallUnit(mbu, 0);
    mbu->id = id;
    MemoryInfo *mi = SmallUnitAlloc(mbu, ms);
    mi->id = id;
    return mi;
}

MemoryInfo *MTMemoryManagerCalloc(MTMemoryManager *mm, MemorySize ms) {
    pthread_t id = pthread_self();
    for (int i = 0; i < mm->mbulistlength; ++i) {
        if (mm->mbulist[i]->id == id) {
            MemoryInfo *m = SmallUnitAlloc(mm->mbulist[i], ms);
            memset(m->m, 0, m->size);
            m->id = id;
            return m;
        }
    }
    MemoryBigUnit *mbu = NewBigUnit(mm);
    NewSmallUnit(mbu, 0);
    mbu->id = id;
    MemoryInfo *mi = SmallUnitAlloc(mbu, ms);
    memset(mi->m, 0, mi->size);
    mi->id = id;
    return mi;
}

MemoryInfo *MTMemoryManagerRealloc(MTMemoryManager *mm, MemoryInfo *mi, MemorySize ms) {
    if (mi->size != ms) {
        pthread_t id = pthread_self();
        for (int i = 0; i < mm->mbulistlength; ++i) {
            if (mm->mbulist[i]->id == id) {
                MemoryInfo *m = SmallUnitAlloc(mm->mbulist[i], ms);
                memcpy(m->m, mi->m, mi->size);
                m->id = id;
                MTMemoryManagerFree(mm, mi);
                return m;
            }
        }
        MemoryBigUnit *mbu = NewBigUnit(mm);
        NewSmallUnit(mbu, 0);
        mbu->id = id;
        MemoryInfo *mi1 = SmallUnitAlloc(mbu, ms);
        memcpy(mi1->m, mi->m, mi->size);
        mi1->id = id;
        MTMemoryManagerFree(mm, mi);
        return mi1;
    }
    return NULL;
}

int MTMemoryManagerFree(MTMemoryManager *mm, MemoryInfo *mi) {
    MemoryBigUnit *mbutmp;
    MemorySmallUnit *msutmp;
    for (int i = 0; i < mm->mbulistlength; ++i) {
        mbutmp = mm->mbulist[i];
        if (mbutmp->id == mi->id) {
            for (int j = 0; j < mbutmp->msulistlength; ++j) {
                msutmp = mbutmp->msulist[j];
                if (msutmp->unit == mi->unit) {
                    return UnitFree(msutmp, mi);
                }
            }
        }
    }
    return 0;
}

MemoryInfo *MTMemoryManagerUnitAlloc(MemoryBigUnit *mbu, MemorySize ms) {
    MemoryInfo *m = SmallUnitAlloc(mbu, ms);
    m->id = mbu->id;
    return m;
}

MemoryInfo *MTMemoryManagerUnitCalloc(MemoryBigUnit *mbu, MemorySize ms) {
    MemoryInfo *m = SmallUnitAlloc(mbu, ms);
    m->id = mbu->id;
    memset(m->m, 0, m->size);
    return m;
}

int MTMemoryManagerUnitFree(MemoryBigUnit *mbu, MemoryInfo *mi) {
    for (int i = 0; i < mbu->msulistlength; ++i) {
        if (mbu->msulist[i]->unit == mi->unit) {
            return UnitFree(mbu->msulist[i], mi);
        }
    }
    return 0;
}

MemoryInfo *MTMemoryManagerUnitRealloc(MemoryBigUnit *mbu, MemoryInfo *mi, MemorySize ms) {
    if (mi->size < ms) {
        MemoryInfo *m = SmallUnitAlloc(mbu, ms);
        m->id = mbu->id;
        memcpy(m->m, mi->m, mi->size);
        for (int i = 0; i < mbu->msulistlength; ++i) {
            if (mbu->msulist[i]->unit == mi->unit) {
                UnitFree(mbu->msulist[i], mi);
            }
        }
        return m;
    } else {
        return NULL;
    }
}

MemoryBigUnit *MTMemoryManagerCreateUnitAndBindingThread(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnit *mbu = NewBigUnit(mm);
    NewSmallUnit(mbu, 0);
    mbu->id = id;
    return mbu;
}

MemoryBigUnit *MTMemoryManagerBindingThread(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnit *mbu;
    for (int i = 0; i < mm->mbulistlength; ++i) {
        mbu = mm->mbulist[i];
        if (mbu->id == 0) {
            mbu->id = id;
            if (mbu->msulistlength == 0) {
                NewSmallUnit(mbu, 0);
            }
            return mbu;
        }
    }
    return MTMemoryManagerCreateUnitAndBindingThread(mm, id);
}

void MTMemoryManagerForceBindingThread(MemoryBigUnit *mbu, pthread_t id) {
    if (mbu->msulistlength == 0) {
        NewSmallUnit(mbu, 0);
    }
    mbu->id = id;
}

void MTMemoryManagerDestroy(MTMemoryManager *mm) {
    MemoryBigUnit *mbutmp;
    for (int i = 0; i < mm->mbulistlength; ++i) {
        mbutmp = mm->mbulist[i];
        for (int j = 0; j < mbutmp->msulistlength; ++j) {
            brk(mbutmp->msulist[j]);
        }
        brk(mbutmp);
    }
    brk(mm->mbulist);
    brk(mm);
}


void MTMemoryManagerManualCleanUnit(MemoryBigUnit *mbu) {
    for (int i = 0; i < mbu->msulistlength; ++i) {
        brk(mbu->msulist[i]);
    }
    mbu->msulistlength = 0;
}

void MTMemoryManagerManualCleanThread(MemoryBigUnit *mbu) {
    mbu->id = 0;
}

int MTMemoryManagerCleanUnit(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnit *mbutmp;
    for (int i = 0; i < mm->mbulistlength; ++i) {
        mbutmp = mm->mbulist[i];
        if (mbutmp->id == id) {
            for (int j = 0; j < mbutmp->msulistlength; ++j) {
                brk(mbutmp->msulist[j]);
            }
            mbutmp->msulistlength = 0;
            return 1;
        }
    }
    return 0;
}

void MTMemoryManagerAppointInitUnit(MemoryBigUnit *mbu) {
    for (int i = 0; i < mbu->msulistlength; ++i) {
        mbu->msulist[i]->freelist = NULL;
        brk(mbu->msulist[i]->freelist);
        mbu->msulist[i]->freelistmake = NULL;
        brk(mbu->msulist[i]->freelistmake);
        mbu->msulist[i] = NULL;
        brk(mbu->msulist[i]);
    }
    mbu->msulistlength = 0;
    mbu->id = 0;
}

int MTMemoryManagerInitUnit(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnit *mbutmp;
    for (int i = 0; i < mm->mbulistlength; ++i) {
        mbutmp = mm->mbulist[i];
        if (mbutmp->id == id) {
            for (int i = 0; i < mbutmp->msulistlength; ++i) {
                brk(mbutmp->msulist[i]);
            }
            mbutmp->msulistlength = 0;
            mbutmp->id = 0;
            return 1;
        }
    }
    return 0;
}

bool MTMemoryManagerCMP(MemoryInfo *mi, MemoryInfo *mk, MemorySize s) {
    return memcmp(mi->m, mk->m, s);
}

void MemoryUnitCheck(MTMemoryManager *mm) {
    MemoryBigUnit *mbutmp;
    MemorySmallUnit *msutmp;
    MemoryInfo *infotmp;
    for (int i = 0; i < mm->mbulistlength; ++i) {
        mbutmp = mm->mbulist[i];
        printf("\n***************************\n");
        printf("bunit:%p id:%lu unit:%zu\n", mbutmp, mbutmp->id, mbutmp->unit);
        for (int j = 0; j < mbutmp->msulistlength; ++j) {
            msutmp = mbutmp->msulist[j];
            printf("////////////////////////\n");
            printf("sunitfreelistsize:%ld\n", msutmp->freelistsize);
            for (int j = 0; j < msutmp->freelistsize; ++j) {
                if (msutmp->freelist[j] == NULL) {
                    printf("sunitfreelist:NULL\n");
                    continue;
                }
                printf("sunitfreelist:%p size:%zu index:%zu\n", msutmp->freelist[j], msutmp->freelist[j]->size,
                       msutmp->freelist[j]->freelistindex);
            }
            printf("////////////////////////\n");
            printf("sunit:%p m:%p size:%zu unit:%zu infohead:%p infotail:%p freesize:%zu length:%zu \n",
                   msutmp, msutmp->m, msutmp->size, msutmp->unit, msutmp->infohead, msutmp->infotail, msutmp->freesize,
                   msutmp->length);
            printf("////////////////////////\n");
            infotmp = msutmp->infohead;
            for (;;) {
                if (infotmp == NULL) {
                    break;
                } else {
                    printf("info:%p mi:%s m:%p back:%p next:%p size:%zu id:%lu unit:%zu isfree:%zu \n",
                           infotmp, ((char *) infotmp->m), infotmp->m, infotmp->back, infotmp->next, infotmp->size,
                           infotmp->id, infotmp->unit, infotmp->isfree);
                    infotmp = infotmp->next;
                }
            }
            printf("\n");
        }
    }
}