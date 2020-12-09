#include "kmp_os.h"

#ifdef __cplusplus
extern "C" {
#endif

KMP_EXTERNAL_INTERFACE
extern omp_allocator_handle_t __kmpc_init_allocator(int gtid,
                                                    omp_memspace_handle_t,
                                                    int ntraits,
                                                    omp_alloctrait_t traits[]);
KMP_EXTERNAL_INTERFACE
extern void __kmpc_destroy_allocator(int gtid, omp_allocator_handle_t al);
KMP_EXTERNAL_INTERFACE
extern void __kmpc_set_default_allocator(int gtid, omp_allocator_handle_t al);
KMP_EXTERNAL_INTERFACE
extern omp_allocator_handle_t __kmpc_get_default_allocator(int gtid);
KMP_EXTERNAL_INTERFACE
extern void *__kmpc_alloc(int gtid, size_t sz, omp_allocator_handle_t al);
KMP_EXTERNAL_INTERFACE
extern void *__kmpc_calloc(int gtid, size_t nmemb, size_t sz,
                           omp_allocator_handle_t al);
KMP_EXTERNAL_INTERFACE
extern void *__kmpc_realloc(int gtid, void *ptr, size_t sz,
                            omp_allocator_handle_t al,
                            omp_allocator_handle_t free_al);
KMP_EXTERNAL_INTERFACE
extern void __kmpc_free(int gtid, void *ptr, omp_allocator_handle_t al);

KMP_EXTERNAL_INTERFACE
extern void __kmpc_dispatch_init_4(ident_t *loc, kmp_int32 gtid,
                                   enum sched_type schedule, kmp_int32 lb,
                                   kmp_int32 ub, kmp_int32 st, kmp_int32 chunk);
KMP_EXTERNAL_INTERFACE
extern void __kmpc_dispatch_init_4u(ident_t *loc, kmp_int32 gtid,
                                    enum sched_type schedule, kmp_uint32 lb,
                                    kmp_uint32 ub, kmp_int32 st,
                                    kmp_int32 chunk);
KMP_EXTERNAL_INTERFACE
extern void __kmpc_dispatch_init_8(ident_t *loc, kmp_int32 gtid,
                                   enum sched_type schedule, kmp_int64 lb,
                                   kmp_int64 ub, kmp_int64 st, kmp_int64 chunk);
KMP_EXTERNAL_INTERFACE
extern void __kmpc_dispatch_init_8u(ident_t *loc, kmp_int32 gtid,
                                    enum sched_type schedule, kmp_uint64 lb,
                                    kmp_uint64 ub, kmp_int64 st,
                                    kmp_int64 chunk);

KMP_EXTERNAL_INTERFACE
extern int __kmpc_dispatch_next_4(ident_t *loc, kmp_int32 gtid,
                                  kmp_int32 *p_last, kmp_int32 *p_lb,
                                  kmp_int32 *p_ub, kmp_int32 *p_st);
KMP_EXTERNAL_INTERFACE
extern int __kmpc_dispatch_next_4u(ident_t *loc, kmp_int32 gtid,
                                   kmp_int32 *p_last, kmp_uint32 *p_lb,
                                   kmp_uint32 *p_ub, kmp_int32 *p_st);
KMP_EXTERNAL_INTERFACE
extern int __kmpc_dispatch_next_8(ident_t *loc, kmp_int32 gtid,
                                  kmp_int32 *p_last, kmp_int64 *p_lb,
                                  kmp_int64 *p_ub, kmp_int64 *p_st);
KMP_EXTERNAL_INTERFACE
extern int __kmpc_dispatch_next_8u(ident_t *loc, kmp_int32 gtid,
                                   kmp_int32 *p_last, kmp_uint64 *p_lb,
                                   kmp_uint64 *p_ub, kmp_int64 *p_st);

KMP_EXTERNAL_INTERFACE
extern void __kmpc_dispatch_fini_4(ident_t *loc, kmp_int32 gtid);
KMP_EXTERNAL_INTERFACE
extern void __kmpc_dispatch_fini_8(ident_t *loc, kmp_int32 gtid);
KMP_EXTERNAL_INTERFACE
extern void __kmpc_dispatch_fini_4u(ident_t *loc, kmp_int32 gtid);
KMP_EXTERNAL_INTERFACE
extern void __kmpc_dispatch_fini_8u(ident_t *loc, kmp_int32 gtid);

// should never have been exported
KMP_EXTERNAL_INTERFACE
KMP_EXPORT int __kmpc_invoke_task_func(int gtid);

KMP_EXTERNAL_INTERFACE
extern kmp_event_t *__kmpc_task_allow_completion_event(ident_t *loc_ref,
                                                       int gtid,
                                                       kmp_task_t *task);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_begin(ident_t *, kmp_int32 flags);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end(ident_t *);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_threadprivate_register_vec(ident_t *, void *data,
                                                  kmpc_ctor_vec ctor,
                                                  kmpc_cctor_vec cctor,
                                                  kmpc_dtor_vec dtor,
                                                  size_t vector_length);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_threadprivate_register(ident_t *, void *data,
                                              kmpc_ctor ctor, kmpc_cctor cctor,
                                              kmpc_dtor dtor);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void *__kmpc_threadprivate(ident_t *, kmp_int32 global_tid,
                                      void *data, size_t size);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_global_thread_num(ident_t *);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_global_num_threads(ident_t *);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_bound_thread_num(ident_t *);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_bound_num_threads(ident_t *);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_ok_to_fork(ident_t *);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_fork_call(ident_t *, kmp_int32 nargs,
                                 kmpc_micro microtask, ...);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_serialized_parallel(ident_t *, kmp_int32 global_tid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end_serialized_parallel(ident_t *, kmp_int32 global_tid);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_flush(ident_t *);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_barrier(ident_t *, kmp_int32 global_tid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_master(ident_t *, kmp_int32 global_tid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end_master(ident_t *, kmp_int32 global_tid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_ordered(ident_t *, kmp_int32 global_tid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end_ordered(ident_t *, kmp_int32 global_tid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_critical(ident_t *, kmp_int32 global_tid,
                                kmp_critical_name *);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end_critical(ident_t *, kmp_int32 global_tid,
                                    kmp_critical_name *);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_critical_with_hint(ident_t *, kmp_int32 global_tid,
                                          kmp_critical_name *, uint32_t hint);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_barrier_master(ident_t *, kmp_int32 global_tid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end_barrier_master(ident_t *, kmp_int32 global_tid);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_barrier_master_nowait(ident_t *,
                                                  kmp_int32 global_tid);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_single(ident_t *, kmp_int32 global_tid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end_single(ident_t *, kmp_int32 global_tid);

KMP_EXPORT void KMPC_FOR_STATIC_INIT(ident_t *loc, kmp_int32 global_tid,
                                     kmp_int32 schedtype, kmp_int32 *plastiter,
                                     kmp_int *plower, kmp_int *pupper,
                                     kmp_int *pstride, kmp_int incr,
                                     kmp_int chunk);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_for_static_fini(ident_t *loc, kmp_int32 global_tid);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_copyprivate(ident_t *loc, kmp_int32 global_tid,
                                   size_t cpy_size, void *cpy_data,
                                   void (*cpy_func)(void *, void *),
                                   kmp_int32 didit);

extern void KMPC_SET_NUM_THREADS(int arg);
extern void KMPC_SET_DYNAMIC(int flag);
extern void KMPC_SET_NESTED(int flag);

/* OMP 3.0 tasking interface routines */
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_omp_task(ident_t *loc_ref, kmp_int32 gtid,
                                     kmp_task_t *new_task);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_task_t *__kmpc_omp_task_alloc(ident_t *loc_ref, kmp_int32 gtid,
                                             kmp_int32 flags,
                                             size_t sizeof_kmp_task_t,
                                             size_t sizeof_shareds,
                                             kmp_routine_entry_t task_entry);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_task_t *__kmpc_omp_target_task_alloc(
    ident_t *loc_ref, kmp_int32 gtid, kmp_int32 flags, size_t sizeof_kmp_task_t,
    size_t sizeof_shareds, kmp_routine_entry_t task_entry, kmp_int64 device_id);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_omp_task_begin_if0(ident_t *loc_ref, kmp_int32 gtid,
                                          kmp_task_t *task);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_omp_task_complete_if0(ident_t *loc_ref, kmp_int32 gtid,
                                             kmp_task_t *task);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_omp_task_parts(ident_t *loc_ref, kmp_int32 gtid,
                                           kmp_task_t *new_task);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_omp_taskwait(ident_t *loc_ref, kmp_int32 gtid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_omp_taskyield(ident_t *loc_ref, kmp_int32 gtid,
                                          int end_part);

#if TASK_UNUSED
KMP_EXTERNAL_INTERFACE
void __kmpc_omp_task_begin(ident_t *loc_ref, kmp_int32 gtid, kmp_task_t *task);
KMP_EXTERNAL_INTERFACE
void __kmpc_omp_task_complete(ident_t *loc_ref, kmp_int32 gtid,
                              kmp_task_t *task);
#endif // TASK_UNUSED

/* ------------------------------------------------------------------------ */

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_taskgroup(ident_t *loc, int gtid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end_taskgroup(ident_t *loc, int gtid);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_omp_task_with_deps(
    ident_t *loc_ref, kmp_int32 gtid, kmp_task_t *new_task, kmp_int32 ndeps,
    kmp_depend_info_t *dep_list, kmp_int32 ndeps_noalias,
    kmp_depend_info_t *noalias_dep_list);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_omp_wait_deps(ident_t *loc_ref, kmp_int32 gtid,
                                     kmp_int32 ndeps,
                                     kmp_depend_info_t *dep_list,
                                     kmp_int32 ndeps_noalias,
                                     kmp_depend_info_t *noalias_dep_list);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_cancel(ident_t *loc_ref, kmp_int32 gtid,
                                   kmp_int32 cncl_kind);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_cancellationpoint(ident_t *loc_ref, kmp_int32 gtid,
                                              kmp_int32 cncl_kind);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_cancel_barrier(ident_t *loc_ref, kmp_int32 gtid);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_proxy_task_completed(kmp_int32 gtid, kmp_task_t *ptask);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_proxy_task_completed_ooo(kmp_task_t *ptask);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_taskloop(ident_t *loc, kmp_int32 gtid, kmp_task_t *task,
                                kmp_int32 if_val, kmp_uint64 *lb,
                                kmp_uint64 *ub, kmp_int64 st, kmp_int32 nogroup,
                                kmp_int32 sched, kmp_uint64 grainsize,
                                void *task_dup);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void *__kmpc_task_reduction_init(int gtid, int num_data, void *data);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void *__kmpc_taskred_init(int gtid, int num_data, void *data);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void *__kmpc_task_reduction_get_th_data(int gtid, void *tg, void *d);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void *__kmpc_task_reduction_modifier_init(ident_t *loc, int gtid,
                                                     int is_ws, int num,
                                                     void *data);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void *__kmpc_taskred_modifier_init(ident_t *loc, int gtid, int is_ws,
                                              int num, void *data);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_task_reduction_modifier_fini(ident_t *loc, int gtid,
                                                    int is_ws);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_omp_reg_task_with_affinity(
    ident_t *loc_ref, kmp_int32 gtid, kmp_task_t *new_task, kmp_int32 naffins,
    kmp_task_affinity_info_t *affin_list);

/* Lock interface routines (fast versions with gtid passed in) */
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_init_lock(ident_t *loc, kmp_int32 gtid,
                                 void **user_lock);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_init_nest_lock(ident_t *loc, kmp_int32 gtid,
                                      void **user_lock);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_destroy_lock(ident_t *loc, kmp_int32 gtid,
                                    void **user_lock);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_destroy_nest_lock(ident_t *loc, kmp_int32 gtid,
                                         void **user_lock);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_set_lock(ident_t *loc, kmp_int32 gtid, void **user_lock);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_set_nest_lock(ident_t *loc, kmp_int32 gtid,
                                     void **user_lock);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_unset_lock(ident_t *loc, kmp_int32 gtid,
                                  void **user_lock);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_unset_nest_lock(ident_t *loc, kmp_int32 gtid,
                                       void **user_lock);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT int __kmpc_test_lock(ident_t *loc, kmp_int32 gtid, void **user_lock);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT int __kmpc_test_nest_lock(ident_t *loc, kmp_int32 gtid,
                                     void **user_lock);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_init_lock_with_hint(ident_t *loc, kmp_int32 gtid,
                                           void **user_lock, uintptr_t hint);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_init_nest_lock_with_hint(ident_t *loc, kmp_int32 gtid,
                                                void **user_lock,
                                                uintptr_t hint);

/* Interface to fast scalable reduce methods routines */

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_reduce_nowait(
    ident_t *loc, kmp_int32 global_tid, kmp_int32 num_vars, size_t reduce_size,
    void *reduce_data, void (*reduce_func)(void *lhs_data, void *rhs_data),
    kmp_critical_name *lck);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end_reduce_nowait(ident_t *loc, kmp_int32 global_tid,
                                         kmp_critical_name *lck);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_reduce(
    ident_t *loc, kmp_int32 global_tid, kmp_int32 num_vars, size_t reduce_size,
    void *reduce_data, void (*reduce_func)(void *lhs_data, void *rhs_data),
    kmp_critical_name *lck);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_end_reduce(ident_t *loc, kmp_int32 global_tid,
                                  kmp_critical_name *lck);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_uint64 __kmpc_get_taskid();
KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_uint64 __kmpc_get_parent_taskid();

// C++ port
// missing 'extern "C"' declarations

KMP_EXTERNAL_INTERFACE
KMP_EXPORT kmp_int32 __kmpc_in_parallel(ident_t *loc);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_pop_num_threads(ident_t *loc, kmp_int32 global_tid);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_push_num_threads(ident_t *loc, kmp_int32 global_tid,
                                        kmp_int32 num_threads);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_push_proc_bind(ident_t *loc, kmp_int32 global_tid,
                                      int proc_bind);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_push_num_teams(ident_t *loc, kmp_int32 global_tid,
                                      kmp_int32 num_teams,
                                      kmp_int32 num_threads);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_fork_teams(ident_t *loc, kmp_int32 argc,
                                  kmpc_micro microtask, ...);
struct kmp_dim { // loop bounds info casted to kmp_int64
  kmp_int64 lo; // lower
  kmp_int64 up; // upper
  kmp_int64 st; // stride
};
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_doacross_init(ident_t *loc, kmp_int32 gtid,
                                     kmp_int32 num_dims,
                                     const struct kmp_dim *dims);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_doacross_wait(ident_t *loc, kmp_int32 gtid,
                                     const kmp_int64 *vec);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_doacross_post(ident_t *loc, kmp_int32 gtid,
                                     const kmp_int64 *vec);
KMP_EXTERNAL_INTERFACE
KMP_EXPORT void __kmpc_doacross_fini(ident_t *loc, kmp_int32 gtid);

KMP_EXTERNAL_INTERFACE
KMP_EXPORT void *__kmpc_threadprivate_cached(ident_t *loc, kmp_int32 global_tid,
                                             void *data, size_t size,
                                             void ***cache);

KMP_EXTERNAL_INTERFACE
extern int __kmpc_get_target_offload();

KMP_EXTERNAL_INTERFACE
extern int __kmpc_pause_resource(kmp_pause_status_t level);

#ifdef __cplusplus
}
#endif
