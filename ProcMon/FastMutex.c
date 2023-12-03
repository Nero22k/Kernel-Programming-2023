#include "pch.h"
#include "FastMutex.h"

void FastMutex_Init(FastMutex* fm) {
    ExInitializeFastMutex(&(fm->m_mutex));
}

void FastMutex_Lock(FastMutex* fm) {
    ExAcquireFastMutex(&(fm->m_mutex));
}

void FastMutex_Unlock(FastMutex* fm) {
    ExReleaseFastMutex(&(fm->m_mutex));
}