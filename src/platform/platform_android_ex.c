#include "quic_platform.h"

//inline
void
CxPlatInternalEventWaitForever(
    _Inout_ CXPLAT_EVENT* Event
)
{
    int Result;

    Result = pthread_mutex_lock(&Event->Mutex);
    CXPLAT_FRE_ASSERT(Result == 0);

    //
    // Spurious wake ups from pthread_cond_wait can occur. So the function needs
    // to be called in a loop until the predicate 'Signalled' is satisfied.
    //

    while (!Event->Signaled)
    {
        Result = pthread_cond_wait(&Event->Cond, &Event->Mutex);
        CXPLAT_FRE_ASSERT(Result == 0);
    }

    if (Event->AutoReset)
    {
        Event->Signaled = false;
    }

    Result = pthread_mutex_unlock(&Event->Mutex);
    CXPLAT_FRE_ASSERT(Result == 0);
}
