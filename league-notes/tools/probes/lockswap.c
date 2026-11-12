// Inserted library that replaces chosen os_unfair_lock entry points with equivalents, to find which of the
// three real ones misbehaves for an app. Build with any of -DSWAP_LOCK -DSWAP_TRY -DSWAP_UNLOCK.
//   lock    -> os_unfair_lock_lock_with_options(l, 0)   (same slow path, different entry)
//   trylock -> compare-and-swap of 0 with the thread's owner tag (%gs:0x18), which is all trylock is
//   unlock  -> os_unfair_lock_unlock_no_tsd(l, owner tag)
#include <os/lock.h>
#include <stdint.h>
#include <stdbool.h>
extern void os_unfair_lock_lock_with_options(os_unfair_lock_t, uint32_t);
extern void os_unfair_lock_unlock_no_tsd(os_unfair_lock_t, uint32_t self);
static inline uint32_t slot3(void) { uint64_t r; __asm__("movq %%gs:0x18, %0" : "=r"(r)); return (uint32_t)r; }
#define INTERPOSE(new, old) __attribute__((used, section("__DATA,__interpose"))) static struct { const void* n; const void* o; } ip_##old = { (const void*)new, (const void*)old }
#ifdef SWAP_LOCK
void swap_lock(os_unfair_lock_t l) { os_unfair_lock_lock_with_options(l, 0); }
INTERPOSE(swap_lock, os_unfair_lock_lock);
#endif
#ifdef SWAP_TRY
bool swap_trylock(os_unfair_lock_t l) { return __sync_bool_compare_and_swap((uint32_t*)l, 0, slot3()); }
INTERPOSE(swap_trylock, os_unfair_lock_trylock);
#endif
#ifdef SWAP_UNLOCK
void swap_unlock(os_unfair_lock_t l) { os_unfair_lock_unlock_no_tsd(l, slot3()); }
INTERPOSE(swap_unlock, os_unfair_lock_unlock);
#endif
