#ifndef _RPY_STMGCINTF_H
#define _RPY_STMGCINTF_H


/* meant to be #included after src_stm/stmgc.h */

#include <errno.h>
#include "stmgc.h"
#include "stm/atomic.h"    /* for spin_loop(), write_fence(), spinlock_xxx() */

extern __thread struct stm_thread_local_s stm_thread_local;
extern __thread long pypy_stm_ready_atomic;
extern __thread uintptr_t pypy_stm_nursery_low_fill_mark;
extern __thread uintptr_t pypy_stm_nursery_low_fill_mark_saved;
/* Invariant: if we're running a transaction:
   - if it is atomic, pypy_stm_nursery_low_fill_mark == (uintptr_t) -1
   - otherwise, if it is inevitable, pypy_stm_nursery_low_fill_mark == 0
   - otherwise, it's a fraction of the nursery size strictly between 0 and 1
*/

void pypy_stm_setup(void);
void pypy_stm_teardown(void);
void pypy_stm_setup_prebuilt(void);        /* generated into stm_prebuilt.c */
void pypy_stm_register_thread_local(void); /* generated into stm_prebuilt.c */
void pypy_stm_unregister_thread_local(void); /* generated into stm_prebuilt.c */

void _pypy_stm_initialize_nursery_low_fill_mark(long v_counter);
void _pypy_stm_inev_state(void);
long _pypy_stm_start_transaction(void);

void _pypy_stm_become_inevitable(const char *);

char *_pypy_stm_test_expand_marker(void);
void pypy_stm_setup_expand_marker(long co_filename_ofs,
                                  long co_name_ofs,
                                  long co_firstlineno_ofs,
                                  long co_lnotab_ofs);

long _pypy_stm_count(void);

/* C8: not implemented properly yet: */
typedef struct {
    stm_thread_local_t *tl;
    char *segment_base;    /* base to interpret the 'object' below */
    uintptr_t odd_number;  /* marker odd number, or 0 if marker is missing */
    object_t *object;      /* marker object, or NULL if marker is missing */
} stm_loc_marker_t;
extern void (*stmcb_light_finalizer)(object_t *o);
extern void (*stmcb_finalizer)(object_t *o);
static inline object_t *stm_allocate_with_finalizer(ssize_t size_rounded_up) {
    return stm_allocate(size_rounded_up);
}
static inline void stm_enable_light_finalizer(object_t *o) {};
static inline int stm_set_timing_log(const char *profiling_file_name,
                       int expand_marker(stm_loc_marker_t *, char *, int)) {return 0;}
/* C8: not implemented properly yet ^^^^^^^^^^^^^^^^^^ */


static inline void pypy_stm_become_inevitable(const char *msg)
{
    assert(STM_SEGMENT->running_thread == &stm_thread_local);
    if (!stm_is_inevitable()) {
        _pypy_stm_become_inevitable(msg);
    }
}
static inline void pypy_stm_commit_if_not_atomic(void) {
    int e = errno;
    if (pypy_stm_ready_atomic == 1) {
        stm_commit_transaction();
    }
    else {
        pypy_stm_become_inevitable("commit_if_not_atomic in atomic");
    }
    errno = e;
}
static inline void pypy_stm_start_if_not_atomic(void) {
    if (pypy_stm_ready_atomic == 1) {
        int e = errno;
        _pypy_stm_start_transaction();
        errno = e;
    }
}
static inline void pypy_stm_start_inevitable_if_not_atomic(void) {
    if (pypy_stm_ready_atomic == 1) {
        int e = errno;
        stm_start_inevitable_transaction(&stm_thread_local);
        _pypy_stm_initialize_nursery_low_fill_mark(0);
        _pypy_stm_inev_state();
        errno = e;
    }
}
static inline void pypy_stm_increment_atomic(void) {
    switch (++pypy_stm_ready_atomic) {
    case 2:
        assert(pypy_stm_nursery_low_fill_mark != (uintptr_t) -1);
        pypy_stm_nursery_low_fill_mark_saved = pypy_stm_nursery_low_fill_mark;
        pypy_stm_nursery_low_fill_mark = (uintptr_t) -1;
        break;
    default:
        break;
    }
}
static inline void pypy_stm_decrement_atomic(void) {
    switch (--pypy_stm_ready_atomic) {
    case 1:
        pypy_stm_nursery_low_fill_mark = pypy_stm_nursery_low_fill_mark_saved;
        assert(pypy_stm_nursery_low_fill_mark != (uintptr_t) -1);
        break;
    case 0:
        pypy_stm_ready_atomic = 1;
        break;
    default:
        break;
    }
}
static inline long pypy_stm_get_atomic(void) {
    return pypy_stm_ready_atomic - 1;
}
long pypy_stm_enter_callback_call(void *);
void pypy_stm_leave_callback_call(void *, long);
void pypy_stm_set_transaction_length(double);
void pypy_stm_transaction_break(void);

static inline int pypy_stm_should_break_transaction(void)
{
    /* we should break the current transaction if we have used more than
       some initial portion of the nursery, or if we are running inevitable
       (in which case pypy_stm_nursery_low_fill_mark is set to 0).
       If the transaction is atomic, pypy_stm_nursery_low_fill_mark is
       instead set to (uintptr_t) -1, and the following check is never true.
    */
    uintptr_t current = (uintptr_t)STM_SEGMENT->nursery_current;
    return current > pypy_stm_nursery_low_fill_mark;
    /* NB. this logic is hard-coded in jit/backend/x86/assembler.py too */
}

static void pypy__rewind_jmp_copy_stack_slice(void)
{
    _rewind_jmp_copy_stack_slice(&stm_thread_local.rjthread);
}


#endif  /* _RPY_STMGCINTF_H */
