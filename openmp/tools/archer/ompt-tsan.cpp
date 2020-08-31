/*
 * ompt-tsan.cpp -- Archer runtime library, TSan annotations for Archer
 */

//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for details.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <list>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if (defined __APPLE__ && defined __MACH__)
#include <dlfcn.h>
#endif

#include "omp-tools.h"
#include <sys/resource.h>
#include <time.h>
#include <sys/time.h>


//#define DEBUG
#ifdef DEBUG
#define DPrintf(...) printf(__VA_ARGS__)
#define DTLCPrintf(...) printf(__VA_ARGS__)
#else
#define DPrintf(...)
#define DTLCPrintf(...)
#endif

#define OPTIMIZE_OWN 1

static int runOnTsan;
static int hasReductionCallback;

class ArcherFlags {
public:
#if (LLVM_VERSION) >= 40
  int flush_shadow;
#endif
  int print_max_rss;
  int verbose;
  int enabled;
  int use_tlc;
  int use_tlc_fibers;
  int use_fibers;
  int use_fiberpool;
  int tasking;
  std::atomic<int> untieds{0};

  ArcherFlags(const char *env)
      :
#if (LLVM_VERSION) >= 40
        flush_shadow(0),
#endif
        print_max_rss(0), verbose(0), enabled(1), use_tlc(0), use_tlc_fibers(0),
        use_fibers(0), use_fiberpool(0), tasking(0) {
    if (env) {
      std::vector<std::string> tokens;
      std::string token;
      std::string str(env);
      std::istringstream iss(str);
      while (std::getline(iss, token, ' '))
        tokens.push_back(token);

      for (std::vector<std::string>::iterator it = tokens.begin();
           it != tokens.end(); ++it) {
#if (LLVM_VERSION) >= 40
        if (sscanf(it->c_str(), "flush_shadow=%d", &flush_shadow))
          continue;
#endif
        if (sscanf(it->c_str(), "print_max_rss=%d", &print_max_rss))
          continue;
        if (sscanf(it->c_str(), "verbose=%d", &verbose))
          continue;
        if (sscanf(it->c_str(), "use_fibers=%d", &use_fibers))
          continue;
        if (sscanf(it->c_str(), "use_fiberpool=%d", &use_fiberpool))
          continue;
        if (sscanf(it->c_str(), "enable=%d", &enabled))
          continue;
        if (sscanf(it->c_str(), "use_tlc=%d", &use_tlc))
          continue;
        if (sscanf(it->c_str(), "use_tlc_fibers=%d", &use_tlc_fibers))
          continue;
        std::cerr << "Illegal values for ARCHER_OPTIONS variable: " << token
                  << std::endl;
      }
    }
    if (use_fibers || use_fiberpool || use_tlc || use_tlc_fibers)
      tasking=1;
  }
};

class TsanFlags {
public:
  int ignore_noninstrumented_modules;

  TsanFlags(const char *env) : ignore_noninstrumented_modules(0) {
    if (env) {
      std::vector<std::string> tokens;
      std::string token;
      std::string str(env);
      std::istringstream iss(str);
      while (std::getline(iss, token, ' '))
        tokens.push_back(token);

      for (std::vector<std::string>::iterator it = tokens.begin();
           it != tokens.end(); ++it) {
        // we are interested in ignore_noninstrumented_modules to print a
        // warning
        if (sscanf(it->c_str(), "ignore_noninstrumented_modules=%d",
                   &ignore_noninstrumented_modules))
          continue;
      }
    }
  }
};

#if (LLVM_VERSION) >= 40
extern "C" {
int __attribute__((weak)) __archer_get_omp_status();
void __attribute__((weak)) __tsan_flush_memory() {}
}
#endif
ArcherFlags *archer_flags;

// The following definitions are pasted from "llvm/Support/Compiler.h" to allow
// the code
// to be compiled with other compilers like gcc:

#ifndef TsanHappensBefore
// Thread Sanitizer is a tool that finds races in code.
// See http://code.google.com/p/data-race-test/wiki/DynamicAnnotations .
// tsan detects these exact functions by name.
extern "C" {
#if (defined __APPLE__ && defined __MACH__)
static void AnnotateHappensAfter(const char *file, int line,
                                 const volatile void *cv) {
  void (*fptr)(const char *, int, const volatile void *);

  fptr = (void (*)(const char *, int, const volatile void *))dlsym(
      RTLD_DEFAULT, "AnnotateHappensAfter");
  (*fptr)(file, line, cv);
}
static void AnnotateHappensBefore(const char *file, int line,
                                  const volatile void *cv) {
  void (*fptr)(const char *, int, const volatile void *);

  fptr = (void (*)(const char *, int, const volatile void *))dlsym(
      RTLD_DEFAULT, "AnnotateHappensBefore");
  (*fptr)(file, line, cv);
}
static void AnnotateIgnoreWritesBegin(const char *file, int line) {
  void (*fptr)(const char *, int);

  fptr = (void (*)(const char *, int))dlsym(RTLD_DEFAULT,
                                            "AnnotateIgnoreWritesBegin");
  (*fptr)(file, line);
}
static void AnnotateIgnoreWritesEnd(const char *file, int line) {
  void (*fptr)(const char *, int);

  fptr = (void (*)(const char *, int))dlsym(RTLD_DEFAULT,
                                            "AnnotateIgnoreWritesEnd");
  (*fptr)(file, line);
}
static void AnnotateNewMemory(const char *file, int line,
                              const volatile void *cv, size_t size) {
  void (*fptr)(const char *, int, const volatile void *, size_t);

  fptr = (void (*)(const char *, int, const volatile void *, size_t))dlsym(
      RTLD_DEFAULT, "AnnotateNewMemory");
  (*fptr)(file, line, cv, size);
}
static void __tsan_func_entry(const void *call_pc) {
  static void (*fptr)(const void *) =
      (void (*)(const void *))dlsym(RTLD_DEFAULT, "__tsan_func_entry");
  if (fptr)
    (*fptr)(call_pc);
}
static void __tsan_func_exit() {
  static void (*fptr)() = (void (*)())dlsym(RTLD_DEFAULT, "__tsan_func_exit");
  if (fptr)
    (*fptr)();
}
static int RunningOnValgrind() {
  int (*fptr)();

  fptr = (int (*)())dlsym(RTLD_DEFAULT, "RunningOnValgrind");
  if (fptr && fptr != RunningOnValgrind)
    runOnTsan = 0;
  return 0;
}
#else
void __attribute__((weak))
AnnotateHappensAfter(const char *file, int line, const volatile void *cv) {}
void __attribute__((weak))
AnnotateHappensBefore(const char *file, int line, const volatile void *cv) {}
void __attribute__((weak))
AnnotateHappensAfterTLC(const char *file, int line, const volatile void *cv) {
  DTLCPrintf("AnnotateHappensAfterTLC fallback to AnnotateHappensAfter\n");
  AnnotateHappensAfter(file, line, cv);
}
void __attribute__((weak))
AnnotateHappensBeforeTLC(const char *file, int line, const volatile void *cv) {
  DTLCPrintf("AnnotateHappensBeforeTLC fallback to AnnotateHappensBefore\n");
  AnnotateHappensBefore(file, line, cv);
}
void __attribute__((weak))
AnnotateInitTLC(const char *file, int line, const volatile void *cv) {
  DTLCPrintf("AnnotateInitTLC fallback to AnnotateHappensBefore\n");
  AnnotateHappensBefore(file, line, cv);
}
void __attribute__((weak))
AnnotateStartTLC(const char *file, int line, const volatile void *cv) {
  DTLCPrintf("AnnotateStartTLC fallback to AnnotateHappensAfter\n");
  AnnotateHappensAfter(file, line, cv);
}
void __attribute__((weak))
AnnotateIgnoreWritesBegin(const char *file, int line) {}
void __attribute__((weak)) AnnotateIgnoreWritesEnd(const char *file, int line) {
}
void __attribute__((weak))
AnnotateNewMemory(const char *file, int line, const volatile void *cv,
                  size_t size) {}
int __attribute__((weak)) RunningOnValgrind() {
  runOnTsan = 0;
  return 0;
}
void __attribute__((weak)) __tsan_func_entry(const void *call_pc) {}
void __attribute__((weak)) __tsan_func_exit(void) {}

void __attribute__((weak)) * __tsan_get_current_fiber() {
  printf("__tsan_get_current_fiber\n");
  return nullptr;
}
void __attribute__((weak)) * __tsan_create_fiber(unsigned flags) {
  printf("__tsan_create_fiber\n");
  return nullptr;
}
void __attribute__((weak)) __tsan_destroy_fiber(void *fiber) {
  printf("__tsan_destroy_fiber\n");
}
void __attribute__((weak)) __tsan_switch_to_fiber(void *fiber, unsigned flags) {
  printf("__tsan_switch_to_fiber\n");
}
unsigned long __attribute__((weak)) __sanitizer_get_current_allocated_bytes() { return 0; }
unsigned long __attribute__((weak)) __sanitizer_get_heap_size() { return 0;}

#endif
}

void *__tlc_get_current_fiber();
void *__tlc_create_fiber(unsigned flags);
void __tlc_destroy_fiber(void *fiber);
void __tlc_switch_to_fiber(void *fiber, unsigned flags);

void *__pool_get_current_fiber();
void *__pool_create_fiber(unsigned flags);
void __pool_destroy_fiber(void *fiber);
void __pool_switch_to_fiber(void *fiber, unsigned flags);

#define TsanGetCurrentFiber()                                                  \
  ((archer_flags->use_fibers)                                                  \
       ? __tsan_get_current_fiber()                                            \
       : ((archer_flags->use_fiberpool) ? __pool_get_current_fiber()           \
       : ((archer_flags->use_tlc_fibers) ? __tlc_get_current_fiber()           \
                                         : nullptr)))
#define TsanCreateFiber(flag)                                                  \
  ((archer_flags->use_fibers)                                                  \
       ? __tsan_create_fiber(flag)                                             \
       : ((archer_flags->use_fiberpool) ? __pool_create_fiber(flag)           \
       : ((archer_flags->use_tlc_fibers) ? __tlc_create_fiber(flag)            \
                                         : nullptr)))
#define TsanDestroyFiber(fiber)                                                \
  do {\
  if (archer_flags->use_fibers) {                                              \
    __tsan_destroy_fiber(fiber);                                               \
  } else if (archer_flags->use_fiberpool) {                                              \
    __pool_destroy_fiber(fiber);                                               \
  } else if (archer_flags->use_tlc_fibers){                                     \
  __tlc_destroy_fiber(fiber);}}while(0)
#define TsanSwitchToFiber(fiber, flag)                                         \
  do {\
  if (archer_flags->use_fibers) {                                              \
    __tsan_switch_to_fiber(fiber, flag);                                       \
  } else if (archer_flags->use_fiberpool) {                                              \
    __pool_switch_to_fiber(fiber, flag);                                               \
  } else if (archer_flags->use_tlc_fibers){                                     \
  __tlc_switch_to_fiber(fiber, flag);}}while(0)

// This marker is used to define a happens-before arc. The race detector will
// infer an arc from the begin to the end when they share the same pointer
// argument.
#define TsanHappensBefore(cv) do{AnnotateHappensBefore(__FILE__, __LINE__, cv);}while(0)

// This marker defines the destination of a happens-before arc.
#define TsanHappensAfter(cv) do{AnnotateHappensAfter(__FILE__, __LINE__, cv);}while(0)

// convenience macros to handle the conditional annotation w/ or w/o TLC
#define IfTLCBefore(f, cv)                                                     \
  do{\
  if (!archer_flags->use_tlc){                                                  \
    AnnotateHappensBefore(__FILE__, __LINE__, cv);                             \
  }else{                                                                         \
    f(__FILE__, __LINE__, cv);\
  }}while(0)

#define IfTLCAfter(f, cv)                                                      \
  do{\
  if (!archer_flags->use_tlc){                                                  \
    AnnotateHappensAfter(__FILE__, __LINE__, cv);                              \
  }else{                                                                         \
    f(__FILE__, __LINE__, cv);\
  }}while(0)

// This marker defines the source of a TLC aware happens-before arc.
#define TsanHappensBeforeTLC(cv) do{IfTLCBefore(AnnotateHappensBeforeTLC, cv);}while(0)

// This marker defines the destination of a TLC aware happens-before arc.
#define TsanHappensAfterTLC(cv) do{IfTLCAfter(AnnotateHappensAfterTLC, cv);}while(0)

// This marker defines the initialization of TLC execution.
#define TsanInitTLC(cv) do{IfTLCBefore(AnnotateInitTLC, cv);}while(0)

// This marker defines the start of TLC execution.
#define TsanStartTLC(cv) do{IfTLCAfter(AnnotateStartTLC, cv);}while(0)

// Ignore any races on writes between here and the next TsanIgnoreWritesEnd.
#define TsanIgnoreWritesBegin() do{AnnotateIgnoreWritesBegin(__FILE__, __LINE__);}while(0)

// Resume checking for racy writes.
#define TsanIgnoreWritesEnd() do{AnnotateIgnoreWritesEnd(__FILE__, __LINE__);}while(0)

// We don't really delete the clock for now
#define TsanDeleteClock(cv)

// newMemory
#define TsanNewMemory(addr, size)                                              \
  AnnotateNewMemory(__FILE__, __LINE__, addr, size)
#define TsanFreeMemory(addr, size)                                             \
  AnnotateNewMemory(__FILE__, __LINE__, addr, size)
#endif

// Function entry/exit
#define TsanFuncEntry(pc) __tsan_func_entry(pc)
#define TsanFuncExit() __tsan_func_exit()

/// Required OMPT inquiry functions.
static ompt_get_parallel_info_t ompt_get_parallel_info;
static ompt_get_thread_data_t ompt_get_thread_data;

typedef int (*ompt_get_task_memory_t)(void **addr, size_t *size, int blocknum);
static ompt_get_task_memory_t ompt_get_task_memory;

typedef char ompt_tsan_clockid;

static uint64_t my_next_id() {
  static uint64_t ID = 0;
  uint64_t ret = __sync_fetch_and_add(&ID, 1);
  return ret;
}

std::mutex outputMutex{};

// Data structure to provide a threadsafe pool of reusable objects.
// DataPool<Type of objects, Size of blockalloc>
template <typename T, int N> struct DataPool {
  std::mutex DPMutex;
  std::vector<T *> DataPointer;
  std::vector<T *> RemoteDataPointer;
  std::list<void *> memory;
  int total;
  std::atomic<int> remote;
  int remoteReturn;
  int localReturn;
  
  virtual int getRemote(){return remoteReturn;}
  virtual int getLocal(){return localReturn;}
  virtual int getTotal(){return total;}
  virtual int getMissing(){return total - DataPointer.size() - RemoteDataPointer.size();}

  virtual void newDatas() {
    if (remote>0) {
      DPMutex.lock();
      remoteReturn++;
      DataPointer.swap(RemoteDataPointer);
      remote=0;
      DPMutex.unlock();
      return;
    }
    // prefix the Data with a pointer to 'this', allows to return memory to
    // 'this',
    // without explicitly knowing the source.
    //
    // To reduce lock contention, we use thread local DataPools, but Data
    // objects move to other threads.
    // The strategy is to get objects from local pool. Only if the object moved
    // to another
    // thread, we might see a penalty on release (returnData).
    // For "single producer" pattern, a single thread creates tasks, these are
    // executed by other threads.
    // The master will have a high demand on TaskData, so return after use.
    struct pooldata {
      DataPool<T, N> *dp;
      T data;
    };
    // We alloc without initialize the memory. We cannot call constructors.
    // Therfore use malloc!
    pooldata *datas = (pooldata *)calloc(N, sizeof(pooldata));
    memory.push_back(datas);
    for (int i = 0; i < N; i++) {
      datas[i].dp = this;
      DataPointer.emplace_back(&(datas[i].data));
    }
    total += N;
  }

  virtual T *getData() {
    T *ret;
    if (DataPointer.empty())
      newDatas();
    ret = DataPointer.back();
    DataPointer.pop_back();
    return ret;
  }

  virtual void returnOwnData(T *data) {
    DataPointer.emplace_back(data);
    localReturn++;
  }

  virtual void returnData(T *data) {
    DPMutex.lock();
    RemoteDataPointer.emplace_back(data);
    remote++;
    remoteReturn++;
    DPMutex.unlock();
  }

  DataPool() : DPMutex(), DataPointer(), total(0), remote(0), remoteReturn(0), localReturn(0) {}

  virtual ~DataPool() {
    // we assume all memory is returned when the thread finished / destructor is
    // called
    for (auto i : memory)
      if (i)
        free(i);
  }
};

// Data structure to provide a threadsafe pool of reusable objects.
// DataPool<Type of objects, Size of blockalloc>
template <typename T, int N> struct PDataPool : public DataPool<T, N> {
  void newDatas() {
    if (this->remote>0) {
      this->DPMutex.lock();
      this->remoteReturn++;
      this->DataPointer.swap(this->RemoteDataPointer);
      this->remote=0;
      this->DPMutex.unlock();
      return;
    }
    // prefix the Data with a pointer to 'this', allows to return memory to
    // 'this',
    // without explicitly knowing the source.
    //
    // To reduce lock contention, we use thread local DataPools, but Data
    // objects move to other threads.
    // The strategy is to get objects from local pool. Only if the object moved
    // to another
    // thread, we might see a penalty on release (returnData).
    // For "single producer" pattern, a single thread creates tasks, these are
    // executed by other threads.
    // The master will have a high demand on TaskData, so return after use.
    struct pooldata {
      DataPool<T, N> *dp;
      T data;
    };
    // We alloc without initialize the memory. We cannot call constructors.
    // Therfore use malloc!
    pooldata *datas = (pooldata *)calloc(N, sizeof(pooldata));
    this->memory.push_back(datas);
    for (int i = 0; i < N; i++) {
      datas[i].dp = this;
      this->DataPointer.push_back(&(datas[i].data));
      datas[i].data.init();
    }
    this->total += N;
  }
  ~PDataPool() {
    for (auto i : this->DataPointer)
      i->fini();
    for (auto i : this->RemoteDataPointer)
      i->fini();
    // we assume all memory is returned when the thread finished / destructor is
    // called
    for (auto i : this->memory)
      if (i)
        free(i);
  }
};

// This function takes care to return the data to the originating DataPool
// A pointer to the originating DataPool is stored just before the actual data.
template <typename T, int N> static void retData(void *data) {
  ((DataPool<T, N> **)data)[-1]->returnData((T *)data);
}

struct FiberData;
static __thread PDataPool<FiberData, 4> *fdp{nullptr};

template<> void retData<FiberData, 4>(void *data) {
  PDataPool<FiberData, 4> * pool = ((PDataPool<FiberData, 4> **)data)[-1];
#ifdef OPTIMIZE_OWN
  if (pool == fdp)
    pool->returnOwnData((FiberData*) data);
  else
#endif
    pool->returnData((FiberData*) data);
}

/// Data structure to store additional information for parallel regions.
struct FiberData {
  static __thread FiberData *currentFiber;
  void *fiber;
  bool isThreadFiber{false};

  void init() { fiber = NULL; }
  void fini() {
    if (fiber!=nullptr && !isThreadFiber)
      __tsan_destroy_fiber(fiber);
  }

  //  void* GetCurrentFiber(){return __tsan_get_current_fiber();}

  void SwitchToFiber(int flags) {
    if (!fiber)
      fiber = __tsan_create_fiber(0);
    __tsan_switch_to_fiber(fiber, flags);
    currentFiber = this;
  }
  static FiberData *getCurrentFiber() {
    if (!currentFiber) {
      currentFiber = new FiberData(__tsan_get_current_fiber());
    }
    return currentFiber;
  }

  FiberData(void *newFiber) {
    if (fiber)
      __tsan_destroy_fiber(fiber);
    isThreadFiber = true;
    fiber = newFiber;
  }
  FiberData() {
    if (!fiber) {
      fiber = __tsan_create_fiber(0);
    }
  }
  ~FiberData() {}
  // overload new/delete to use DataPool for memory management.
  void *operator new(size_t size) { return fdp->getData(); }
  void operator delete(void *p, size_t) { retData<FiberData, 4>(p); }
};
__thread FiberData *FiberData::currentFiber{nullptr};

struct ParallelData;
__thread DataPool<ParallelData, 4> *pdp;

/// Data structure to store additional information for parallel regions.
struct ParallelData {

  // Parallel fork is just another barrier, use Barrier[1]

  /// Two addresses for relationships with barriers.
  ompt_tsan_clockid Barrier[2];

  const void *codePtr;

  void *GetParallelPtr() { return &(Barrier[1]); }

  void *GetBarrierPtr(unsigned Index) { return &(Barrier[Index]); }

  ParallelData(const void *codeptr) : codePtr(codeptr) {}
  ~ParallelData() {
    TsanDeleteClock(&(Barrier[0]));
    TsanDeleteClock(&(Barrier[1]));
  }
  // overload new/delete to use DataPool for memory management.
  void *operator new(size_t size) { return pdp->getData(); }
  void operator delete(void *p, size_t) { retData<ParallelData, 4>(p); }
};

static inline ParallelData *ToParallelData(ompt_data_t *parallel_data) {
  return reinterpret_cast<ParallelData *>(parallel_data->ptr);
}

struct Taskgroup;
__thread DataPool<Taskgroup, 4> *tgp;

/// Data structure to support stacking of taskgroups and allow synchronization.
struct Taskgroup {
  /// Its address is used for relationships of the taskgroup's task set.
  ompt_tsan_clockid Ptr;

  /// Reference to the parent taskgroup.
  Taskgroup *Parent;

  Taskgroup(Taskgroup *Parent) : Parent(Parent) {}
  ~Taskgroup() { TsanDeleteClock(&Ptr); }

  void *GetPtr() { return &Ptr; }
  // overload new/delete to use DataPool for memory management.
  void *operator new(size_t size) { return tgp->getData(); }
  void operator delete(void *p, size_t) { retData<Taskgroup, 4>(p); }
};

struct TaskData;
__thread DataPool<TaskData, 4> *tdp;

template<> void retData<TaskData, 4>(void *data) {
  DataPool<TaskData, 4> * pool = ((DataPool<TaskData, 4> **)data)[-1];
#ifdef OPTIMIZE_OWN
  if (pool == tdp)
    pool->returnOwnData((TaskData*) data);
  else
#endif
    pool->returnData((TaskData*) data);
}

/// Data structure to store additional information for tasks.
struct TaskData {
  /// Its address is used for relationships of this task.
  ompt_tsan_clockid Task;

  /// Child tasks use its address to declare a relationship to a taskwait in
  /// this task.
  ompt_tsan_clockid Taskwait;

  /// Whether this task is currently executing a barrier.
  bool InBarrier;

  /// Whether this task is an included task.
  int TaskType{0};

  /// Index of which barrier to use next.
  char BarrierIndex;

  /// Count how often this structure has been put into child tasks + 1.
  std::atomic_int RefCount;

  /// Reference to the parent that created this task.
  TaskData *Parent;

  /// Reference to the implicit task in the stack above this task.
  TaskData *ImplicitTask;

  /// Reference to the team of this task.
  ParallelData *Team;

  /// Reference to the current taskgroup that this task either belongs to or
  /// that it just created.
  Taskgroup *TaskGroup;

  /// Dependency information for this task.
  ompt_dependence_t *Dependencies;

  /// Number of dependency entries.
  unsigned DependencyCount;

  void *PrivateData;
  size_t PrivateDataSize;

  int execution;
  int freed;

  void *fiber;

  TaskData(TaskData *Parent, int taskType)
      : InBarrier(false), TaskType(taskType), BarrierIndex(0), RefCount(1),
        Parent(Parent), ImplicitTask(nullptr), Team(Parent->Team),
        TaskGroup(nullptr), DependencyCount(0), execution(0), freed(0),
        fiber(nullptr) {
    if (Parent != nullptr) {
      Parent->RefCount++;
      // Copy over pointer to taskgroup. This task may set up its own stack
      // but for now belongs to its parent's taskgroup.
      TaskGroup = Parent->TaskGroup;
      fiber = TsanCreateFiber(0);
    }
  }

  TaskData(ParallelData *Team, int taskType)
      : InBarrier(false), TaskType(taskType), BarrierIndex(0), RefCount(1),
        Parent(nullptr), ImplicitTask(this), Team(Team), TaskGroup(nullptr),
        DependencyCount(0), execution(1), freed(0), fiber(nullptr) {
        fiber = TsanGetCurrentFiber();
  }

  ~TaskData() {
    TsanDeleteClock(&Task);
    TsanDeleteClock(&Taskwait);
    if (ImplicitTask != this && fiber){
      TsanDestroyFiber(fiber);
    }
  }

  void Activate() {
      TsanSwitchToFiber(fiber, 1);
  }
  
  bool isIncluded() { return TaskType & ompt_task_undeferred; }
  bool isUntied() { return TaskType & ompt_task_untied; }
  bool isFinal() { return TaskType & ompt_task_final; }
  bool isMergable() { return TaskType & ompt_task_mergeable; }
  bool isMerged() { return TaskType & ompt_task_merged; }

  bool isExplicit() { return TaskType & ompt_task_explicit; }
  bool isImplicit() { return TaskType & ompt_task_implicit; }
  bool isInitial() { return TaskType & ompt_task_initial; }
  bool isTarget() { return TaskType & ompt_task_target; }

  void *GetTaskPtr() { return &Task; }

  void *GetTaskwaitPtr() { return &Taskwait; }
  // overload new/delete to use DataPool for memory management.
  void *operator new(size_t size) { return tdp->getData(); }
  void operator delete(void *p, size_t) { retData<TaskData, 4>(p); }
};

static inline TaskData *ToTaskData(ompt_data_t *task_data) {
  return reinterpret_cast<TaskData *>(task_data->ptr);
}

struct TlcFiber;
__thread DataPool<TlcFiber, 4> *tfp;

template<> void retData<TlcFiber, 4>(void *data) {
  DataPool<TlcFiber, 4> * pool = ((DataPool<TlcFiber, 4> **)data)[-1];
#ifdef OPTIMIZE_OWN
  if (pool == tfp)
    pool->returnOwnData((TlcFiber*) data);
  else
#endif
    pool->returnData((TlcFiber*) data);
}

/// Data structure to support stacking of taskgroups and allow synchronization.
struct TlcFiber {
  /// Its address is used for relationships of the taskgroup's task set.
  ompt_tsan_clockid Ptr;

  void *GetPtr() { return &Ptr; }
  // overload new/delete to use DataPool for memory management.
  void *operator new(size_t size) { return tfp->getData(); }
  void operator delete(void *p, size_t) { retData<TlcFiber, 4>(p); }
};

union FiberImpl{
  TlcFiber* tlc;
  FiberData* pool;
  void* tsan;
};
__thread FiberImpl Fiber{nullptr};

void *__tlc_get_current_fiber() {
  DTLCPrintf("__tlc_get_current_fiber\n");
  if (!Fiber.tlc)
    Fiber.tlc = new TlcFiber;
  return Fiber.tlc;
}
void *__tlc_create_fiber(unsigned flags) {
  DTLCPrintf("__tlc_create_fiber\n");
  TlcFiber *ret = new TlcFiber;
  AnnotateInitTLC(__FILE__, __LINE__, ret->GetPtr());
  return ret;
}
void __tlc_destroy_fiber(void *fiber) {
  DTLCPrintf("__tlc_destroy_fiber\n");
  delete reinterpret_cast<TlcFiber *>(fiber);
}
void __tlc_switch_to_fiber(void *fiber, unsigned flags) {
  DTLCPrintf("__tlc_switch_fiber\n");
  if (Fiber.tlc)
    TsanHappensBefore(Fiber.tlc->GetPtr());
  Fiber.tlc = reinterpret_cast<TlcFiber *>(fiber);
  AnnotateStartTLC(__FILE__, __LINE__, Fiber.tlc->GetPtr());
}

void *__pool_get_current_fiber() {
  DTLCPrintf("__pool_get_current_fiber\n");
  if (!Fiber.pool)
    Fiber.pool = FiberData::getCurrentFiber();
  return Fiber.pool;
}
void *__pool_create_fiber(unsigned flags) {
  DTLCPrintf("__pool_create_fiber\n");
  FiberData *ret = new FiberData();
  return ret;
}
void __pool_destroy_fiber(void *fiber) {
  DTLCPrintf("__pool_destroy_fiber\n");
  delete reinterpret_cast<FiberData *>(fiber);
}
void __pool_switch_to_fiber(void *fiber, unsigned flags) {
  DTLCPrintf("__pool_switch_fiber\n");
  Fiber.pool = reinterpret_cast<FiberData *>(fiber);
  Fiber.pool->SwitchToFiber(flags);
}

static inline void *ToInAddr(void *OutAddr) {
  // FIXME: This will give false negatives when a second variable lays directly
  //        behind a variable that only has a width of 1 byte.
  //        Another approach would be to "negate" the address or to flip the
  //        first bit...
  return reinterpret_cast<char *>(OutAddr) + 1;
}

/// Store a mutex for each wait_id to resolve race condition with callbacks.
std::unordered_map<ompt_wait_id_t, std::mutex> Locks;
std::mutex LocksMutex;

static void ompt_tsan_thread_begin(ompt_thread_t thread_type,
                                   ompt_data_t *thread_data) {
  pdp = new DataPool<ParallelData, 4>;
  TsanNewMemory(pdp, sizeof(pdp));
  tgp = new DataPool<Taskgroup, 4>;
  TsanNewMemory(tgp, sizeof(tgp));
  tdp = new DataPool<TaskData, 4>;
  TsanNewMemory(tdp, sizeof(tdp));
  if (archer_flags->use_tlc_fibers) {
    tfp = new DataPool<TlcFiber, 4>;
    TsanNewMemory(tfp, sizeof(tfp));
  }
  if (archer_flags->use_fiberpool) {
    fdp = new PDataPool<FiberData, 4>;
    TsanNewMemory(fdp, sizeof(fdp));
    TsanGetCurrentFiber();
  }
  thread_data->value = my_next_id();
}

static void ompt_tsan_thread_end(ompt_data_t *thread_data) {
  if (archer_flags->print_max_rss) {
    outputMutex.lock();
    printf("Bytes allocated by TSan heap_size: %lu, current_allocated_bytes: %lu\n", __sanitizer_get_heap_size(), __sanitizer_get_current_allocated_bytes() );
    std::cout << thread_data->value << "tdp: " << tdp->getLocal() << " remote " << tdp->getRemote() << " total " << tdp->getTotal() << " missing " << tdp->getMissing() << std::endl;
    if (archer_flags->use_tlc_fibers)
      std::cout << thread_data->value << "tfp: " << tfp->getLocal() << " remote " << tfp->getRemote() << " total " << tfp->getTotal() << " missing " << tfp->getMissing() << std::endl;
    if (archer_flags->use_fiberpool)
      std::cout << thread_data->value << "fdp: " << fdp->getLocal() << " remote " << fdp->getRemote() << " total " << fdp->getTotal() << " missing " << fdp->getMissing() << std::endl;
    outputMutex.unlock();
  }
  TsanIgnoreWritesBegin();
  delete pdp;
  delete tgp;
  delete tdp;
  if (tfp) {
    delete tfp;
  }
  if (fdp) {
    delete fdp;
  }
  TsanIgnoreWritesEnd();
}

/// OMPT event callbacks for handling parallel regions.

static void ompt_tsan_parallel_begin(ompt_data_t *parent_task_data,
                                     const ompt_frame_t *parent_task_frame,
                                     ompt_data_t *parallel_data,
                                     uint32_t requested_team_size, int flag,
                                     const void *codeptr_ra) {
  ParallelData *Data = new ParallelData(codeptr_ra);
  parallel_data->ptr = Data;

  TsanHappensBefore(Data->GetParallelPtr());
}

static void ompt_tsan_parallel_end(ompt_data_t *parallel_data,
                                   ompt_data_t *task_data, int flag,
                                   const void *codeptr_ra) {
  ParallelData *Data = ToParallelData(parallel_data);
  TsanHappensAfter(Data->GetBarrierPtr(0));
  TsanHappensAfter(Data->GetBarrierPtr(1));

  delete Data;

#if (LLVM_VERSION >= 40)
  if (&__archer_get_omp_status) {
    if (__archer_get_omp_status() == 0 && archer_flags->flush_shadow)
      __tsan_flush_memory();
  }
#endif
}

static void ompt_tsan_implicit_task(ompt_scope_endpoint_t endpoint,
                                    ompt_data_t *parallel_data,
                                    ompt_data_t *task_data,
                                    unsigned int team_size,
                                    unsigned int thread_num, int type) {
  switch (endpoint) {
  case ompt_scope_begin:
    if (type & ompt_task_initial) {
      parallel_data->ptr = new ParallelData(nullptr);
    }
    task_data->ptr = new TaskData(ToParallelData(parallel_data), type);
    TsanHappensAfter(ToParallelData(parallel_data)->GetParallelPtr());
    TsanFuncEntry(ToParallelData(parallel_data)->codePtr);
    break;
  case ompt_scope_end:
    TaskData *Data = ToTaskData(task_data);
#ifdef DEBUG
    assert(Data->freed == 0 && "Implicit task end should only be called once!");
    Data->freed = 1;
#endif
    assert(Data->RefCount == 1 &&
           "All tasks should have finished at the implicit barrier!");
    delete Data;
    TsanFuncExit();
    break;
  }
}

static void ompt_tsan_sync_region(ompt_sync_region_t kind,
                                  ompt_scope_endpoint_t endpoint,
                                  ompt_data_t *parallel_data,
                                  ompt_data_t *task_data,
                                  const void *codeptr_ra) {
  TaskData *Data = ToTaskData(task_data);
  switch (endpoint) {
  case ompt_scope_begin:
    TsanFuncEntry(codeptr_ra);
    switch (kind) {
    case ompt_sync_region_barrier_implementation:
    case ompt_sync_region_barrier_implicit:
    case ompt_sync_region_barrier_explicit:
    case ompt_sync_region_barrier: {
      char BarrierIndex = Data->BarrierIndex;
      TsanHappensBefore(Data->Team->GetBarrierPtr(BarrierIndex));

      if (hasReductionCallback < ompt_set_always) {
        // We ignore writes inside the barrier. These would either occur during
        // 1. reductions performed by the runtime which are guaranteed to be
        // race-free.
        // 2. execution of another task.
        // For the latter case we will re-enable tracking in task_switch.
        Data->InBarrier = true;
        TsanIgnoreWritesBegin();
      }

      break;
    }

    case ompt_sync_region_taskwait:
      break;

    case ompt_sync_region_taskgroup:
      Data->TaskGroup = new Taskgroup(Data->TaskGroup);
      break;

    default:
      break;
    }
    break;
  case ompt_scope_end:
    TsanFuncExit();
    switch (kind) {
    case ompt_sync_region_barrier_implementation:
    case ompt_sync_region_barrier_implicit:
    case ompt_sync_region_barrier_explicit:
    case ompt_sync_region_barrier: {
      if (hasReductionCallback < ompt_set_always) {
        // We want to track writes after the barrier again.
        Data->InBarrier = false;
        TsanIgnoreWritesEnd();
      }

      char BarrierIndex = Data->BarrierIndex;
      // Barrier will end after it has been entered by all threads.
      if (parallel_data) {
        TsanHappensAfter(Data->Team->GetBarrierPtr(BarrierIndex));
      }

      // It is not guaranteed that all threads have exited this barrier before
      // we enter the next one. So we will use a different address.
      // We are however guaranteed that this current barrier is finished
      // by the time we exit the next one. So we can then reuse the first
      // address.
      Data->BarrierIndex = (BarrierIndex + 1) % 2;
      break;
    }

    case ompt_sync_region_taskwait: {
      if (Data->execution > 1) {
        TsanHappensAfter(Data->GetTaskwaitPtr());
      }
      break;
    }

    case ompt_sync_region_taskgroup: {
      assert(Data->TaskGroup != nullptr &&
             "Should have at least one taskgroup!");

      TsanHappensAfter(Data->TaskGroup->GetPtr());

      // Delete this allocated taskgroup, all descendent task are finished by
      // now.
      Taskgroup *Parent = Data->TaskGroup->Parent;
      delete Data->TaskGroup;
      Data->TaskGroup = Parent;
      break;
    }

    default:
      break;
    }
    break;
  }
}

static void ompt_tsan_reduction(ompt_sync_region_t kind,
                                ompt_scope_endpoint_t endpoint,
                                ompt_data_t *parallel_data,
                                ompt_data_t *task_data,
                                const void *codeptr_ra) {
  switch (endpoint) {
  case ompt_scope_begin:
    switch (kind) {
    case ompt_sync_region_reduction:
      TsanIgnoreWritesBegin();
      break;
    default:
      break;
    }
    break;
  case ompt_scope_end:
    switch (kind) {
    case ompt_sync_region_reduction:
      TsanIgnoreWritesEnd();
      break;
    default:
      break;
    }
    break;
  }
}

/// OMPT event callbacks for handling tasks.

static void ompt_tsan_task_create(
    ompt_data_t *parent_task_data,    /* id of parent task            */
    const ompt_frame_t *parent_frame, /* frame data for parent task   */
    ompt_data_t *new_task_data,       /* id of created task           */
    int type, int has_dependences,
    const void *codeptr_ra) /* pointer to outlined function */
{
  TaskData *Data;
  assert(new_task_data->ptr == NULL &&
         "Task data should be initialized to NULL");
  if (type & ompt_task_initial) {
    ompt_data_t *parallel_data;
    int team_size = 1;
    ompt_get_parallel_info(0, &parallel_data, &team_size);
    ParallelData *PData = new ParallelData(nullptr);
    parallel_data->ptr = PData;

    Data = new TaskData(PData, type);
    new_task_data->ptr = Data;
  } else if (type & ompt_task_explicit || type & ompt_task_target) {
    Data = new TaskData(ToTaskData(parent_task_data), type);
    new_task_data->ptr = Data;

    if (!Data->isIncluded()) {
      // Use the newly created address. We cannot use a single address from the
      // parent because that would declare wrong relationships with other
      // sibling tasks that may be created before this task is started!
      TsanInitTLC(Data->GetTaskPtr());
      ToTaskData(parent_task_data)->execution++;
    }
  }
  if (! archer_flags->untieds++ && (archer_flags->use_fiberpool || archer_flags->use_fibers) && Data->isUntied())
    fprintf(stderr, "Archer Warning: fiber based analysis not yet supported for untied tasks\n");
}

static void __ompt_tsan_release_task(TaskData *task) {
  while (task != nullptr && --task->RefCount == 0) {
    TaskData *Parent = task->Parent;
    if (task->DependencyCount > 0) {
      delete[] task->Dependencies;
    }
    delete task;
    task = Parent;
  }
}

static void __ompt_tsan_release_dependencies(TaskData *task) {
  for (unsigned i = 0; i < task->DependencyCount; i++) {
    ompt_dependence_t *Dependency = &task->Dependencies[i];

    // in dependencies block following inout and out dependencies!
    TsanHappensBefore(ToInAddr(Dependency->variable.ptr));
    if (Dependency->dependence_type == ompt_dependence_type_out || Dependency->dependence_type == ompt_dependence_type_inout) {
      TsanHappensBefore(Dependency->variable.ptr);
    }
  }
}

static void __ompt_tsan_acquire_dependencies(TaskData *task) {
  for (unsigned i = 0; i < task->DependencyCount; i++) {
    ompt_dependence_t *Dependency = &task->Dependencies[i];

    TsanHappensAfter(Dependency->variable.ptr);
    // in and inout dependencies are also blocked by prior in dependencies!
    if (Dependency->dependence_type == ompt_dependence_type_out ||
        Dependency->dependence_type == ompt_dependence_type_inout) {
      TsanHappensAfter(ToInAddr(Dependency->variable.ptr));
    }
  }
}


static void ompt_tsan_task_schedule(ompt_data_t *first_task_data,
                                    ompt_task_status_t prior_task_status,
                                    ompt_data_t *second_task_data) {

  //
  //  The necessary action depends on prior_task_status:
  //
  //    ompt_task_early_fulfill = 5,
  //     -> ignored
  //
  //    ompt_task_cancel        = 3,
  //     -> first freed, second ignored
  //
  //    ompt_task_late_fulfill  = 6,
  //     -> first completed, first freed, second ignored
  //
  //    ompt_task_complete      = 1,
  //     -> first completed, first freed, second starts
  //
  //    ompt_task_detach        = 4,
  //    ompt_task_yield         = 2,
  //    ompt_task_switch        = 7
  //     -> first suspended, second starts
  //

  if (prior_task_status == ompt_task_early_fulfill)
    return;

  TaskData *FromTask = ToTaskData(first_task_data);

  if (prior_task_status == ompt_task_cancel) {
    // free the cancelled task
    __ompt_tsan_release_task(FromTask);
    return;
  }

  // Legacy handling for missing reduction callback

  if (hasReductionCallback < ompt_set_always && FromTask->InBarrier) {
    // We want to ignore writes in the runtime code during barriers,
    // but not when executing tasks with user code!
    TsanIgnoreWritesEnd();
  }

  // The late fulfill happens after the detached task finished execution
  if (prior_task_status == ompt_task_late_fulfill)
    TsanHappensAfter(FromTask->GetTaskPtr());

  // task completed execution
  if (prior_task_status == ompt_task_complete ||
      prior_task_status == ompt_task_late_fulfill) {
    // Included tasks are executed sequentially, no need to track
    // synchronization
    if (!FromTask->isIncluded()) {
      // Task will finish before a barrier in the surrounding parallel region
      // ...
      ParallelData *PData = FromTask->Team;
      TsanHappensBefore(
          PData->GetBarrierPtr(FromTask->ImplicitTask->BarrierIndex));

      // ... and before an eventual taskwait by the parent thread.
      TsanHappensBefore(FromTask->Parent->GetTaskwaitPtr());

      if (FromTask->TaskGroup != nullptr) {
        // This task is part of a taskgroup, so it will finish before the
        // corresponding taskgroup_end.
        TsanHappensBefore(FromTask->TaskGroup->GetPtr());
      }
    }

    // release dependencies
    __ompt_tsan_release_dependencies(FromTask);
    if (prior_task_status == ompt_task_complete && !FromTask->isIncluded() && !FromTask->isUntied())
      ToTaskData(second_task_data)->Activate(); // must switch to next task before deleting the previous
    if (archer_flags->tasking && ompt_get_task_memory) {
      void *addr;
      size_t size;
      int ret_task_memory = 1, block=0;
      while (ret_task_memory) {
        ret_task_memory = ompt_get_task_memory(&addr, &size, block);
        if (size>0)
          TsanNewMemory(((void**)addr), size+8);
      }
    }
    // free the previously running task
    __ompt_tsan_release_task(FromTask);
  } else {
    TaskData *ToTask = ToTaskData(second_task_data);
    if (!(ToTask->isIncluded() && ToTask->execution == 0) && !ToTask->isUntied() && !FromTask->isUntied())
      ToTask->Activate(); // must switch to next task before deleting the previous
  }

  // For late fulfill of detached task, there is no task to schedule to
  if (prior_task_status == ompt_task_late_fulfill) {
    return;
  }


  TaskData *ToTask = ToTaskData(second_task_data);
  // Legacy handling for missing reduction callback
  if (hasReductionCallback < ompt_set_always && ToTask->InBarrier) {
    // We re-enter runtime code which currently performs a barrier.
    TsanIgnoreWritesBegin();
  }

  // task suspended
  if (prior_task_status == ompt_task_switch ||
      prior_task_status == ompt_task_yield ||
      prior_task_status == ompt_task_detach) {
    // Task may be resumed at a later point in time.
    TsanInitTLC(FromTask->GetTaskPtr());
    ToTask->ImplicitTask = FromTask->ImplicitTask;
    assert(ToTask->ImplicitTask != NULL &&
           "A task belongs to a team and has an implicit task on the stack");
  }

  // Handle dependencies on first execution of the task
  if (ToTask->execution == 0) {
    if (archer_flags->tasking) {
      if (ompt_get_task_memory) {
        void *addr;
        size_t size;
        int ret_task_memory = 1, block=0;
        while (ret_task_memory) {
          ret_task_memory = ompt_get_task_memory(&addr, &size, block);
          if (size>0)
            TsanNewMemory(((void**)addr), size+8);
            DTLCPrintf("TsanNewMemory(%p, %li)\n", addr, size);
        }
      }
      TsanNewMemory((char*)__builtin_frame_address(0)-1024, 1024);
    }
    ToTask->execution++;
    if (!archer_flags->use_tlc_fibers) {
      // 1. Task will begin execution after it has been created.
      TsanStartTLC(ToTask->GetTaskPtr());
    }
    __ompt_tsan_acquire_dependencies(ToTask);
  } else {
    // 2. Task will resume after it has been switched away.
    if (!archer_flags->use_tlc_fibers) {
      TsanStartTLC(ToTask->GetTaskPtr());
    }
   }
}



static void ompt_tsan_dependences(ompt_data_t *task_data,
                                  const ompt_dependence_t *deps, int ndeps) {
  if (ndeps > 0) {
    // Copy the data to use it in task_switch and task_end.
    TaskData *Data = ToTaskData(task_data);
    Data->Dependencies = new ompt_dependence_t[ndeps];
    std::memcpy(Data->Dependencies, deps, sizeof(ompt_dependence_t) * ndeps);
    Data->DependencyCount = ndeps;

    // This callback is executed before this task is first started.
    TsanHappensBefore(Data->GetTaskPtr());
  }
}

/// OMPT event callbacks for handling locking.
static void ompt_tsan_mutex_acquired(ompt_mutex_t kind, ompt_wait_id_t wait_id,
                                     const void *codeptr_ra) {

  // Acquire our own lock to make sure that
  // 1. the previous release has finished.
  // 2. the next acquire doesn't start before we have finished our release.
  LocksMutex.lock();
  std::mutex &Lock = Locks[wait_id];
  LocksMutex.unlock();

  Lock.lock();
  TsanHappensAfter(&Lock);
}

static void ompt_tsan_mutex_released(ompt_mutex_t kind, ompt_wait_id_t wait_id,
                                     const void *codeptr_ra) {
  LocksMutex.lock();
  std::mutex &Lock = Locks[wait_id];
  LocksMutex.unlock();
  TsanHappensBefore(&Lock);

  Lock.unlock();
}

// callback , signature , variable to store result , required support level
#define SET_OPTIONAL_CALLBACK_T(event, type, result, level)                    \
  do {                                                                         \
    ompt_callback_##type##_t tsan_##event = &ompt_tsan_##event;                \
    result = ompt_set_callback(ompt_callback_##event,                          \
                               (ompt_callback_t)tsan_##event);                 \
    if (result < level)                                                        \
      printf("Registered callback '" #event "' is not supported at " #level    \
             " (%i)\n",                                                        \
             result);                                                          \
  } while (0)

#define SET_CALLBACK_T(event, type)                                            \
  do {                                                                         \
    int res;                                                                   \
    SET_OPTIONAL_CALLBACK_T(event, type, res, ompt_set_always);                \
  } while (0)

#define SET_CALLBACK(event) SET_CALLBACK_T(event, event)

static int ompt_tsan_initialize(ompt_function_lookup_t lookup, int device_num,
                                ompt_data_t *tool_data) {
  struct timespec start;
  clock_gettime(CLOCK_REALTIME, &start);
  tool_data->value=start.tv_sec*1000000000+start.tv_nsec;
  
  const char *options = getenv("TSAN_OPTIONS");
  TsanFlags tsan_flags(options);

  ompt_set_callback_t ompt_set_callback =
      (ompt_set_callback_t)lookup("ompt_set_callback");
  if (ompt_set_callback == NULL) {
    std::cerr << "Could not set callback, exiting..." << std::endl;
    std::exit(1);
  }
  ompt_get_parallel_info =
      (ompt_get_parallel_info_t)lookup("ompt_get_parallel_info");
  ompt_get_thread_data = (ompt_get_thread_data_t)lookup("ompt_get_thread_data");
  ompt_get_task_memory = (ompt_get_task_memory_t)lookup("ompt_get_task_memory");

  if (ompt_get_parallel_info == NULL) {
    fprintf(stderr, "Could not get inquiry function 'ompt_get_parallel_info', "
                    "exiting...\n");
    exit(1);
  }

  SET_CALLBACK(thread_begin);
  SET_CALLBACK(thread_end);
if (archer_flags->enabled) {
  SET_CALLBACK(parallel_begin);
  SET_CALLBACK(implicit_task);
  SET_CALLBACK(sync_region);
  SET_CALLBACK(parallel_end);

  SET_CALLBACK(task_create);
  SET_CALLBACK(task_schedule);
  SET_CALLBACK(dependences);

  SET_CALLBACK_T(mutex_acquired, mutex);
  SET_CALLBACK_T(mutex_released, mutex);
  SET_OPTIONAL_CALLBACK_T(reduction, sync_region, hasReductionCallback,
                          ompt_set_never);
}
  if (!tsan_flags.ignore_noninstrumented_modules)
    fprintf(stderr,
            "Warning: please export "
            "TSAN_OPTIONS='ignore_noninstrumented_modules=1' "
            "to avoid false positive reports from the OpenMP runtime.!\n");
  return 1; // success
}

static void ompt_tsan_finalize(ompt_data_t *tool_data) {
  if (archer_flags->print_max_rss) {
    struct rusage end;
    getrusage(RUSAGE_SELF, &end);
    printf("MAX RSS[KBytes] during execution: %ld\n", end.ru_maxrss);

    struct timespec stop;
    clock_gettime(CLOCK_REALTIME, &stop);
    printf("Execution time: %09li ns\n", stop.tv_sec*1000000000+stop.tv_nsec - tool_data->value);
  }

  if (archer_flags)
    delete archer_flags;
}

extern "C" ompt_start_tool_result_t *
ompt_start_tool(unsigned int omp_version, const char *runtime_version) {
  const char *options = getenv("ARCHER_OPTIONS");
  archer_flags = new ArcherFlags(options);
/*  if (!archer_flags->enabled) {
    if (archer_flags->verbose)
      std::cout << "Archer disabled, stopping operation" << std::endl;
    delete archer_flags;
    return NULL;
  }*/

  static ompt_start_tool_result_t ompt_start_tool_result = {
      &ompt_tsan_initialize, &ompt_tsan_finalize, {0}};
  runOnTsan = 1;
  RunningOnValgrind();
  if (!runOnTsan) // if we are not running on TSAN, give a different tool the
  // chance to be loaded
  {
    if (archer_flags->verbose)
      std::cout << "Archer detected OpenMP application without TSan "
                   "stopping operation"
                << std::endl;
    delete archer_flags;
    return NULL;
  }

  if (archer_flags->verbose) {
    std::cout << "Archer detected OpenMP application with TSan, supplying "
                 "OpenMP synchronization semantics"
              << std::endl;
    if (!archer_flags->use_tlc) {
      std::cout << "Running in non-TLC mode" << std::endl;
    } else {
      std::cout << "Running in TLC mode" << std::endl;
    }
  }
  return &ompt_start_tool_result;
}
