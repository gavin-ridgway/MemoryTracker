#ifndef MEMORYTRACKERIMPL_H_
#define MEMORYTRACKERIMPL_H_
//#include "MemoryTracker.h"

bool cppt::MemoryTracker::logging = false;
cppt::MemoryTracker::MemoryMap *cppt::MemoryTracker::allocMap = 0;
bool cppt::MemoryTracker::tracking = false;
std::string *cppt::MemoryTracker::cmd = 0;

namespace
{
#if defined(__GNUC__) // Needed to ensure that MemoryTracker is constructed first.
    __attribute__ ((init_priority (101)))
#endif
    cppt::MemoryTracker m;
}

void *operator new(std::size_t count) noexcept(false)
{
    void *SP;
    GET_STACKPOINTER(SP);

    // Save stack pointer for caller of this function.
    return cppt::MemoryTracker::track(count,
        static_cast<cppt::MemoryTracker::Address>(RETURN_ADDRESS(SP)), false);
}

void *operator new[](std::size_t count) noexcept(false)
{
    void *SP;
    GET_STACKPOINTER(SP);

    // Save stack pointer for caller of this function.
    return cppt::MemoryTracker::track(count,
        static_cast<cppt::MemoryTracker::Address>(RETURN_ADDRESS(SP)), true);
}

void operator delete(void *ptr) throw()
{
    void *SP;
    GET_STACKPOINTER(SP);

    cppt::MemoryTracker::untrack(ptr, 
        static_cast<cppt::MemoryTracker::Address>(RETURN_ADDRESS(SP)), false);
}

void operator delete[](void *ptr) throw()
{
    void *SP;
    GET_STACKPOINTER(SP);

    cppt::MemoryTracker::untrack(ptr,
        static_cast<cppt::MemoryTracker::Address>(RETURN_ADDRESS(SP)), true);
}


#endif /*MEMORYTRACKERIMPL_H_*/
