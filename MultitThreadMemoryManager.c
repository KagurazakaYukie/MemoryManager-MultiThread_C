#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include "MultitThreadMemoryManager.h"

MTMemoryManager *MTMemoryManagerInit(ThreadUnit Length, UnitSize us) {
    MemoryBigUnitList *mbul = sbrk(0);
    brk(mbul + sizeof(MemoryBigUnitList));
    mbul->bnext = NULL;
    mbul->mbu = sbrk(0);
    brk(mbul->mbu + ((sizeof(MemoryBigUnit) * Length) + sizeof(MemoryBigUnit) * 3));
    mbul->length = Length + 3;
    mbul->zero = Length;

    MTMemoryManager *mm = sbrk(0);
    brk(mm + sizeof(MTMemoryManager));
    mm->mbu = NULL;
    mm->msusize = us;
    mm->listsize = Length;
    mm->mbu = mbul;
    mm->mbuw = mbul;

    MemoryBigUnit *mbu;
    mbu = sbrk(0);
    brk(mbu + MemoryBigUnitSize);
    mbu->snext = NULL;
    mbu->msuw = NULL;
    mbu->id = pthread_self();
    mbu->unit = 0;

    NewSmallUnit(mbu, us);

    mbul->mbu[0] = mbu;

    for (size_t i = 1; i < Length; i++) {
        mbu = sbrk(0);
        brk(mbu + MemoryBigUnitSize);
        mbu->snext = NULL;
        mbu->msuw = NULL;
        mbu->id = 0;
        mbu->unit = 0;
        mbul->mbu[i] = mbu;
    }
    return mm;
}

MemoryBigUnit *NewBigUnit(MTMemoryManager *mm) {
    MemoryBigUnitList *mbul = mm->mbuw;
    if (mbul->length != mbul->zero) {
        MemoryBigUnit *mbu;
        mbu = sbrk(0);
        brk(mbu + MemoryBigUnitSize);
        mbu->snext = NULL;
        mbu->msuw = NULL;
        mbu->id = 0;
        mbu->unit = 0;
        mbul->mbu[mbul->zero] = mbu;
        mbul->zero++;
        return mbu;
    } else {
        MemoryBigUnitList *mbul1 = sbrk(0);
        brk(mbul1 + sizeof(MemoryBigUnitList));
        mbul1->bnext = NULL;
        mbul1->mbu = sbrk(0);
        brk(mbul1->mbu + ((sizeof(MemoryBigUnit) * mm->listsize) + sizeof(MemoryBigUnit) * 3));
        mbul1->length = mm->listsize + 3;
        mbul1->zero = 0;
        mbul->bnext = mbul1;
        mm->mbuw = mbul1;
        return NewBigUnit(mm);
    }
}

MemorySmallUnit *NewSmallUnit(MemoryBigUnit *mbu, UnitSize us) {
    void *msu = sbrk(0);
    brk(msu + MemorySmallUnitSize + us);
    MemorySmallUnit *mu = (MemorySmallUnit *) msu;
    mu->m = msu + MemorySmallUnitSize;
    mu->minfo = NULL;
    mu->minfow = NULL;
    mu->unit = mbu->unit++;
    mu->length = 0;
    mu->fsize = 0;
    mu->size = us;
    mu->next = NULL;
    if (mbu->snext == NULL) {
        mbu->snext = mu;
        mbu->msuw = mu;
    } else {
        mbu->msuw->next = mu;
        mbu->msuw = mu;
    }
    return mu;
}

MemoryInfo *UnitMalloc(MemorySmallUnit *mu, MemorySize s) {
    if (mu->minfo != NULL && mu->fsize >= s + MemoryInfoSize) {
        MemoryInfo *m = mu->minfo;
        while (true) {
            if (m == NULL) {
                break;
            } else {
                if (m->size < s) {
                    m = m->next;
                    continue;
                }
                if (m->isfree == 1) {
                    if (m->size == s) {
                        m->isfree = 0;
                        mu->fsize -= m->size + MemoryInfoSize;
                        return m;
                    }
                    if (m->size > s + MemoryInfoSize) {
                        m->size -= s + MemoryInfoSize;
                        mu->fsize -= s + MemoryInfoSize;
                        MemoryInfo *mi = (MemoryInfo *) (m->m + m->size);
                        mi->m = (m->m + m->size + MemoryInfoSize);
                        mi->unit = mu->unit;
                        mi->size = s;
                        mi->isfree = 0;
                        mi->next = m->next;
                        mi->back = m;
                        if (m->next != NULL) {
                            m->next->back = mi;
                        }
                        if (mu->minfow == m) {
                            mu->minfow = mi;
                        }
                        m->next = mi;
                        return mi;
                    }
                }
                m = m->next;
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
        mi->back = mu->minfow;
        if (mu->minfo == NULL) {
            mu->minfo = mi;
            mu->minfow = mi;
        } else {
            mu->minfow->next = mi;
            mu->minfow = mi;
        }
        return mi;
    }
    return NULL;
}

void UnitFree(MemorySmallUnit *mu, MemoryInfo *m) {
    if (m->isfree == 0) {
        m->isfree = 1;
        mu->fsize += (m->size + MemoryInfoSize);
        MemoryInfo *mi = m->back, *mw = NULL, *miw = NULL;
        int next = 0;
        if (mi != NULL) {
            if (mi->isfree == 1) {
                miw = (mi->m + mi->size);
                if (miw == m) {
                    mi->size += m->size + MemoryInfoSize;
                    mi->next = m->next;
                    if (m->next != NULL) {
                        m->next->back = mi;
                    }
                    mu->minfow = mu->minfow == m ? mi : mu->minfow;
                    mu->minfo = mu->minfo == m ? mi : mu->minfo;

                    if (m->next != NULL) {
                        if (m->next->isfree == 1) {
                            mw = (m->m + m->size);
                            if (mw == m->next) {
                                mi->size += m->next->size + MemoryInfoSize;
                                mi->next = m->next->next;
                                if (m->next->next != NULL) {
                                    m->next->next->back = mi;
                                }
                                mu->minfow = mu->minfow == m->next ? mi : mu->minfow;
                                mu->minfo = mu->minfo == m->next ? mi : mu->minfo;
                                next = 1;
                            }
                        }
                    }

                }
            }
        }

        if (next == 0) {
            mi = m->next;
            if (mi != NULL) {
                if (mi->isfree == 1) {
                    mw = (m->m + m->size);
                    if (mw == mi) {
                        m->size += mi->size + MemoryInfoSize;
                        m->next = mi->next;
                        if (mi->next != NULL) {
                            mi->next->back = m;
                        }
                        mu->minfow = mu->minfow == mi ? m : mu->minfow;
                        mu->minfo = mu->minfo == mi ? m : mu->minfo;
                    }
                }
            }
        }
    }
}

MemoryInfo *SmallUnitAlloc(MTMemoryManager *mm, MemoryBigUnit *mbu, MemorySmallUnit *msu, MemorySize ms) {
    MemorySmallUnit *mu = msu;
    while (true) {
        if (mu == NULL) {
            break;
        } else {
            if (mu->size - mu->length >= ms + MemoryInfoSize
                || mu->fsize >= ms + MemoryInfoSize) {
                return UnitMalloc(mu, ms);
            } else {
                mu = mu->next;
            }
        }
    }
    return UnitMalloc(NewSmallUnit(mbu, mm->msusize), ms);
}

MemoryInfo *MTMemoryManagerAlloc(MTMemoryManager *mm, MemorySize ms) {
    pthread_t id = pthread_self();
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                if (mbul->mbu[i]->id == id) {
                    MemoryInfo *m = SmallUnitAlloc(mm, mbul->mbu[i], mbul->mbu[i]->snext, ms);
                    m->id = id;
                    return m;
                }
            }
            mbul = mbul->bnext;
        }
    }
    MemoryBigUnit *mbu = NewBigUnit(mm);
    NewSmallUnit(mbu, mm->msusize);
    mbu->id = id;
    MemoryInfo *mi = SmallUnitAlloc(mm, mbu, mbu->snext, ms);
    mi->id = id;
    return mi;
}

MemoryInfo *MTMemoryManagerCalloc(MTMemoryManager *mm, MemorySize ms) {
    pthread_t id = pthread_self();
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                if (mbul->mbu[i]->id == id) {
                    MemoryInfo *m = SmallUnitAlloc(mm, mbul->mbu[i], mbul->mbu[i]->snext, ms);
                    memset(m->m, 0, (size_t) m->size);
                    m->id = id;
                    return m;
                }
            }
            mbul = mbul->bnext;
        }
    }
    MemoryBigUnit *mbu = NewBigUnit(mm);
    NewSmallUnit(mbu, mm->msusize);
    mbu->id = id;
    MemoryInfo *mi = SmallUnitAlloc(mm, mbu, mbu->snext, ms);
    memset(mi->m, 0, (size_t) mi->size);
    mi->id = id;
    return mi;
}

MemoryInfo *MTMemoryManagerRealloc(MTMemoryManager *mm, MemoryInfo *mi, MemorySize ms) {
    if (mi->size != ms) {
        pthread_t id = pthread_self();
        MemoryBigUnitList *mbul = mm->mbu;
        while (true) {
            if (mbul == NULL) {
                break;
            } else {
                for (size_t i = 0; i < mbul->zero; i++) {
                    if (mbul->mbu[i]->id == id) {
                        MemoryInfo *m = SmallUnitAlloc(mm, mbul->mbu[i], mbul->mbu[i]->snext, ms);
                        memcpy(m->m, mi->m, mi->size);
                        m->id = id;
                        MTMemoryManagerFree(mm, mi);
                        return m;
                    }
                }
                mbul = mbul->bnext;
            }
        }
        MemoryBigUnit *mbu = NewBigUnit(mm);
        NewSmallUnit(mbu, mm->msusize);
        mbu->id = id;
        MemoryInfo *mi1 = SmallUnitAlloc(mm, mbu, mbu->snext, ms);
        memcpy(mi1->m, mi->m, mi->size);
        mi1->id = id;
        MTMemoryManagerFree(mm, mi);
        return mi1;
    } else {
        return mi;
    }
}

void MTMemoryManagerFree(MTMemoryManager *mm, MemoryInfo *mi) {
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                if (mbul->mbu[i]->id == mi->id) {
                    MemorySmallUnit *msu = mbul->mbu[i]->snext;
                    while (true) {
                        if (msu == NULL) {
                            break;
                        } else {
                            if (msu->unit == mi->unit) {
                                return UnitFree(msu, mi);
                            }
                        }
                        msu = msu->next;
                    }
                }
            }
            mbul = mbul->bnext;
        }
    }
}

MemoryInfo *MTMemoryManagerUnitAlloc(MTMemoryManager *mm, MemoryBigUnit *mbu, MemorySize ms) {
    MemoryInfo *m = SmallUnitAlloc(mm, mbu, mbu->snext, ms);
    m->id = mbu->id;
    return m;
}

MemoryInfo *MTMemoryManagerUnitCalloc(MTMemoryManager *mm, MemoryBigUnit *mbu, MemorySize ms) {
    MemoryInfo *m = SmallUnitAlloc(mm, mbu, mbu->snext, ms);
    m->id = mbu->id;
    memset(m->m, 0, (size_t) m->size);
    return m;
}

void MTMemoryManagerUnitFree(MemoryBigUnit *mbu, MemoryInfo *mi) {
    MemorySmallUnit *msu = mbu->snext;
    while (true) {
        if (msu == NULL) {
            break;
        } else {
            if (msu->unit == mi->unit) {
                UnitFree(msu, mi);
                break;
            }
            msu = msu->next;
        }
    }
}

MemoryInfo *MTMemoryManagerUnitRealloc(MTMemoryManager *mm, MemoryBigUnit *mu, MemoryInfo *mi, MemorySize ms) {
    if (mi->size != ms) {
        int asd = mi->size;
        void *aa = mi->m;
        MTMemoryManagerUnitFree(mu, mi);
        MemoryInfo *h = MTMemoryManagerUnitAlloc(mm, mu, ms);
        memcpy(h->m, aa, asd);
        return h;
    } else {
        return mi;
    }
}

MemoryBigUnit *MTMemoryManagerCreateUnitAndBindingThread(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnit *mbu = NewBigUnit(mm);
    NewSmallUnit(mbu, mm->msusize);
    mbu->id = id;
    return mbu;
}

MemoryBigUnit *MTMemoryManagerBindingThread(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                if (mbul->mbu[i]->id == 0) {
                    mbul->mbu[i]->id = id;
                    if (mbul->mbu[i]->snext == NULL) {
                        NewSmallUnit(mbul->mbu[i], mm->msusize);
                    }
                    return mbul->mbu[i];
                }
            }
            mbul = mbul->bnext;
        }
    }
    return MTMemoryManagerCreateUnitAndBindingThread(mm, id);
}

void MTMemoryManagerManualBindingThread(MTMemoryManager *mm, MemoryBigUnit *mbu, pthread_t id) {
    mbu->id = id;
    if (mbu->snext == NULL) {
        NewSmallUnit(mbu, mm->msusize);
    }
}

void MTMemoryManagerDestroy(MTMemoryManager *mm) {
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                MemorySmallUnit *msu = mbul->mbu[i]->snext;
                while (true) {
                    if (msu == NULL) {
                        break;
                    } else {
                        msu = msu->next;
                        brk(msu);
                    }
                }
                brk(mbul->mbu[i]);
            }
            mbul = mbul->bnext;
            brk(mbul);
        }
    }
    brk(mm);
}


void MTMemoryManagerManualCleanUnit(MemoryBigUnit *mbu) {
    MemorySmallUnit *msu = mbu->snext;
    while (true) {
        if (msu == NULL) {
            break;
        } else {
            msu = msu->next;
            brk(msu);
        }
    }
}

void MTMemoryManagerManualCleanThread(MemoryBigUnit *mbu) {
    mbu->id = 0;
}

void MTMemoryManagerCleanUnit(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                if (mbul->mbu[i]->id == id) {
                    MemorySmallUnit *msu = mbul->mbu[i]->snext;
                    while (true) {
                        if (msu == NULL) {
                            break;
                        } else {
                            msu = msu->next;
                            brk(msu);
                        }
                    }
                }
            }
            mbul = mbul->bnext;
        }
    }
}

void MTMemoryManagerCleanThread(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                if (mbul->mbu[i]->id == id) {
                    mbul->mbu[i]->id = 0;
                }
            }
            mbul = mbul->bnext;
        }
    }
}

void MTMemoryManagerAppointComleteInitUnit(MemoryBigUnit *mbu) {
    MemoryInfo *info = mbu->snext->minfo;
    while (true) {
        if (info == NULL) {
            break;
        } else {
            UnitFree(mbu->snext, info);
            info->id = 0;
            info = info->next;
        }
    }
    MemorySmallUnit *msu = mbu->snext->next;
    while (true) {
        if (msu == NULL) {
            break;
        } else {
            msu = msu->next;
            brk(msu);
        }
    }
    mbu->snext->next = NULL;
    mbu->id = 0;
}

void MTMemoryManagerCompleteInitUnit(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                if (mbul->mbu[i]->id == id) {
                    MemoryInfo *info = mbul->mbu[i]->snext->minfo;
                    while (true) {
                        if (info == NULL) {
                            break;
                        } else {
                            UnitFree(mbul->mbu[i]->snext, info);
                            info->id = 0;
                            info = info->next;
                        }
                    }
                    MemorySmallUnit *msu = mbul->mbu[i]->snext->next;
                    while (true) {
                        if (msu == NULL) {
                            break;
                        } else {
                            msu = msu->next;
                            brk(msu);
                        }
                    }
                    mbul->mbu[i]->snext->next = NULL;
                    mbul->mbu[i]->id = 0;
                    return;
                }
            }
            mbul = mbul->bnext;
        }
    }
}

void MTMemoryManagerInitUnit(MTMemoryManager *mm, pthread_t id) {
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                if (mbul->mbu[i]->id == id) {
                    MemoryInfo *info = mbul->mbu[i]->snext->minfo;
                    while (true) {
                        if (info == NULL) {
                            break;
                        } else {
                            UnitFree(mbul->mbu[i]->snext, info);
                            info->id = 0;
                            info = info->next;
                        }
                    }
                    MemorySmallUnit *msu = mbul->mbu[i]->snext->next;
                    while (true) {
                        if (msu == NULL) {
                            break;
                        } else {
                            msu = msu->next;
                            brk(msu);
                        }
                    }
                    mbul->mbu[i]->snext->next = NULL;
                    mbul->mbu[i]->id = 0;
                    return;
                }
            }
            mbul = mbul->bnext;
        }
    }
}

bool MTMemoryManagerCMP(MemoryInfo *mi, MemoryInfo *mk, MemorySize s) {
    if (memcmp(mi->m, mk->m, s) == 0) {
        return true;
    } else {
        return false;
    }
}

void MemoryUnitCheck(MTMemoryManager *mm) {
    MemoryBigUnitList *mbul = mm->mbu;
    while (true) {
        if (mbul == NULL) {
            break;
        } else {
            for (size_t i = 0; i < mbul->zero; i++) {
                printf("\n\n***************************\n");
                printf("bunit:%p id:%lu unitlength:%ld snext:%p msuw:%p \n", mbul->mbu[i], mbul->mbu[i]->id,
                       mbul->mbu[i]->unit,
                       mbul->mbu[i]->snext, mbul->mbu[i]->msuw);
                MemorySmallUnit *msu = mbul->mbu[i]->snext;
                while (true) {
                    if (msu == NULL) {
                        break;
                    } else {
                        printf("////////////////////////\n");
                        printf("sunit:%p m:%p next:%p size:%ld unit:%ld minfo:%p minfow:%p fsize:%ld length:%ld \n",
                               msu,
                               msu->m, msu->next, msu->size, msu->unit, msu->minfo, msu->minfow, msu->fsize,
                               msu->length);
                        printf("////////////////////////\n");
                        MemoryInfo *mi = msu->minfo;
                        while (true) {
                            if (mi == NULL) {
                                break;
                            } else {
                                printf("info:%p mi:%s m:%p back:%p next:%p size:%ld id:%lu unit:%ld isfree:%d \n", mi,
                                       ((char *) mi->m), mi->m,
                                       mi->back, mi->next, mi->size, mi->id, mi->unit, mi->isfree);
                                mi = mi->next;
                            }
                        }
                        printf("\n\n");
                        msu = msu->next;
                    }
                }
            }
            mbul = mbul->bnext;
        }
    }
}
