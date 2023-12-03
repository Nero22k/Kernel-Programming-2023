#pragma once

typedef struct _FastMutex {
    FAST_MUTEX m_mutex;
} FastMutex;

void FastMutex_Init(FastMutex* fm);
void FastMutex_Lock(FastMutex* fm);
void FastMutex_Unlock(FastMutex* fm);