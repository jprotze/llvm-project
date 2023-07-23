#include <algorithm>
#include <atomic>
#include <deque>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

#include <assert.h>
#include <dlfcn.h>

namespace __fiber {

static void (*AnnotateHappensAfter)(const char *, int, const volatile void *);
static void (*AnnotateHappensBefore)(const char *, int, const volatile void *);
#define TsanHappensBeforeStore(cv) AnnotateHappensBefore(__FILE__, __LINE__, cv)
#define TsanHappensAfterStore(cv) AnnotateHappensAfter(__FILE__, __LINE__, cv)
static void (*AnnotateIgnoreSyncBegin)(const char *, int);
static void (*AnnotateIgnoreSyncEnd)(const char *, int);
#define TsanIgnoreSyncBegin() AnnotateIgnoreSyncBegin(__FILE__, __LINE__)
#define TsanIgnoreSyncEnd() AnnotateIgnoreSyncEnd(__FILE__, __LINE__)

static void *(*__tsan_get_current_fiber)();
static void *(*__tsan_create_fiber)(unsigned flags);
static void (*__tsan_destroy_fiber)(void *fiber);
static void (*__tsan_switch_to_fiber)(void *fiber, unsigned flags);
static void (*__tsan_set_fiber_name)(void *fiber, const char *name);
static int (*__tsan_get_fiber_stacksize)(void *fiber);

static std::atomic<int> pagesize{4096};
static std::atomic<int> fibers{0};

class FiberFlags {
public:
  int lazy{0};
  int direct{0};
  int verbose{0};

  FiberFlags(const char *env) {
    if (env) {
      std::vector<std::string> tokens;
      std::string str(env);
      auto end = str.end();
      auto it = str.begin();
      auto is_sep = [](char c) {
        return c == ' ' || c == ',' || c == ':' || c == '\n' || c == '\t' ||
               c == '\r';
      };
      while (it != end) {
        auto next_it = std::find_if(it, end, is_sep);
        tokens.emplace_back(it, next_it);
        it = next_it;
        if (it != end) {
          ++it;
        }
      }

      for (const auto &token : tokens) {
        if (sscanf(token.c_str(), "lazy=%d", &lazy))
          continue;
        if (sscanf(token.c_str(), "direct=%d", &direct))
          continue;
        if (sscanf(token.c_str(), "verbose=%d", &verbose))
          continue;
        std::cerr << "Illegal values for FIBER_OPTIONS variable: " << token
                  << std::endl;
      }
    }
  }
};
static FiberFlags *fiber_flags;

// Data structure to provide a threadsafe pool of reusable objects.
// DataPool<Type of objects>
template <typename T> class DataPool final {
private:
  std::mutex DPMutex{};

  // store unused objects
  std::deque<T *> DataPointer{};
  std::deque<T *> RemoteDataPointer{};

  // store all allocated memory to finally release
  std::list<void *> memory;

  // count remotely returned data (RemoteDataPointer.size())
  std::atomic<int> remote{0};

  // totally allocated data objects in pool
  int total{0};

public:
  static __thread DataPool<T> *ThreadDataPool;

  int getTotal() { return total; }
  int getMissing() {
    return total - DataPointer.size() - RemoteDataPointer.size();
  }

  // fill the pool by allocating a page of memory
  void newDatas() {
    if (remote > total / 8) {
      DataPointer.shrink_to_fit();
      {
        const std::lock_guard<std::mutex> lock(DPMutex);
        // DataPointer is empty, so just swap the vectors
        DataPointer.swap(RemoteDataPointer);
        remote = 0;
      }
      DataPointer.shrink_to_fit();
      return;
    }
    // calculate size of an object including padding to cacheline size
    size_t elemSize = sizeof(T);
    size_t paddedSize = (((elemSize - 1) / 64) + 1) * 64;
    // number of padded elements to allocate
    int ndatas = pagesize / paddedSize;
    char *datas = (char *)malloc(ndatas * paddedSize);
    memory.push_back(datas);
    for (int i = 0; i < ndatas; i++) {
      DataPointer.push_back(new (datas + i * paddedSize) T(this));
    }
    total += ndatas;
  }

  // get data from the pool
  T *getData() {
    T *ret;
    if (DataPointer.empty())
      newDatas();
    assert(!DataPointer.empty());
    ret = DataPointer.back();
    DataPointer.pop_back();
    return ret;
  }

  // accesses to the thread-local datapool don't need locks
  void returnOwnData(T *data) { DataPointer.emplace_front(data); }

  // returning to a remote datapool using lock
  void returnData(T *data) {
    const std::lock_guard<std::mutex> lock(DPMutex);
    RemoteDataPointer.emplace_back(data);
    remote++;
  }

  ~DataPool() {
    // we assume all memory is returned when the thread finished / destructor is
    // called
    for (auto i : DataPointer)
      if (i)
        i->~T();
    for (auto i : RemoteDataPointer)
      if (i)
        i->~T();
    for (auto i : memory)
      if (i)
        free(i);
  }
};

class FiberApi {
public:
  virtual void SwitchToFiber(unsigned flags, const char* file, int line) = 0;
  virtual void SetFiberName(const char *name) = 0;
  virtual int GetFiberStacksize() = 0;
  virtual void Delete() = 0;
  virtual ~FiberApi() {}
};

template <typename T> class DataPoolEntry : public FiberApi {
public:
  DataPool<T> *owner;

  static T *New() { return DataPool<T>::ThreadDataPool->getData(); }

  void Delete() override {
    static_cast<T *>(this)->Reset();
    if (owner == DataPool<T>::ThreadDataPool)
      owner->returnOwnData(static_cast<T *>(this));
    else
      owner->returnData(static_cast<T *>(this));
  }

  DataPoolEntry(DataPool<T> *dp) : owner(dp) {}
  virtual ~DataPoolEntry() {}
};

class FiberData;
typedef DataPool<FiberData> FiberDataPool;
template <> __thread FiberDataPool *FiberDataPool::ThreadDataPool = nullptr;

/// Data structure to wrap a TSan fiber.
class FiberData final : public DataPoolEntry<FiberData> {
public:
  static __thread FiberData *currentFiber;
  void *fiber{nullptr};
  bool isThreadFiber{false};
  const char *switchFile{nullptr}, *createFile{nullptr};
  int switchLine{0}, createLine{0};

  void SwitchToFiber(unsigned flags, const char* file, int line) override {
    switchFile = file;
    switchLine = line;
    if (!fiber) {
      if (flags)
        TsanIgnoreSyncBegin();
      fiber = __tsan_create_fiber(flags);
      fibers++;
      if (flags)
        TsanIgnoreSyncEnd();
    }
    __tsan_switch_to_fiber(fiber, flags);
    currentFiber = this;
  }
  void SetFiberName(const char *name) override {
    __tsan_set_fiber_name(fiber, name);
  }
  int GetFiberStacksize() override {
    return __tsan_get_fiber_stacksize(fiber);
  }
  static FiberData *getCurrentFiber() {
    assert(currentFiber);
    /*if (!currentFiber) {
      currentFiber = FiberData::New(__tsan_get_current_fiber());
    }*/
    return currentFiber;
  }

  FiberData *Init(void *newFiber) {
    if (fiber && !isThreadFiber) {
      __tsan_destroy_fiber(fiber);
    }
    isThreadFiber = true;
    fiber = newFiber;
    return this;
  }
  FiberData *Init(unsigned flags, const char* file, int line) {
    if (!fiber || isThreadFiber) {
      isThreadFiber = false;
      if (flags)
        TsanIgnoreSyncBegin();
      fiber = __tsan_create_fiber(flags);
      fibers++;
      if (flags)
        TsanIgnoreSyncEnd();
    }
    createFile = file;
    createLine = line;
    return this;
  }

  void Reset() {
    createFile = nullptr;
    createLine = 0;
    switchFile = nullptr;
    switchLine = 0;
  }

  static FiberData *New(unsigned flags, const char* file, int line) {
    return DataPoolEntry<FiberData>::New()->Init(flags, file, line);
  }
  static FiberData *New(void *newFiber) {
    return DataPoolEntry<FiberData>::New()->Init(newFiber);
  }

  FiberData(DataPool<FiberData> *dp) : DataPoolEntry<FiberData>(dp) {}
  ~FiberData() {
    if (fiber != nullptr && !isThreadFiber)
      __tsan_destroy_fiber(fiber);
    fiber = nullptr;
  }
};
__thread FiberData *FiberData::currentFiber{nullptr};

class LazyFiberData;
typedef DataPool<LazyFiberData> LazyFiberDataPool;
template <>
__thread LazyFiberDataPool *LazyFiberDataPool::ThreadDataPool = nullptr;

/// Data structure to lazily wrap a TSan fiber.
class LazyFiberData final : public DataPoolEntry<LazyFiberData> {
private:
  FiberData *fiber{nullptr};
  bool isThreadFiber{false};
  std::string name{};
  const char *switchFile{nullptr}, *createFile{nullptr};
  int switchLine{0}, createLine{0};

public:
  static __thread LazyFiberData *currentFiber;

  void SwitchToFiber(unsigned flags, const char* file, int line) override {
    switchFile = file;
    switchLine = line;
    assert(!fiber_flags->direct && fiber_flags->lazy);
    bool isNew{false};
    if (!fiber) {
      fiber = FiberData::New(flags, createFile, createLine);
      if (!name.empty())
        fiber->SetFiberName(name.c_str());
      isNew = true;
    }
    fiber->SwitchToFiber(flags, file, line);
    if (isNew)
      TsanHappensAfterStore(&fiber);
    currentFiber = this;
  }
  void SetFiberName(const char *name) override {
    assert(!fiber_flags->direct && fiber_flags->lazy);
    if (fiber)
      return fiber->SetFiberName(name);
    this->name = std::string{name};
  }
  int GetFiberStacksize() override {
    assert(!fiber_flags->direct && fiber_flags->lazy);
    assert(fiber);
    return fiber->GetFiberStacksize();
  }
  static LazyFiberData *getCurrentFiber() {
    assert(!fiber_flags->direct && fiber_flags->lazy);
    assert(currentFiber);
    /*if (!currentFiber) {
      currentFiber = LazyFiberData::New(FiberData::getCurrentFiber());
    }*/
    return currentFiber;
  }

  LazyFiberData *Init(FiberData *newFiber) {
    assert(!fiber_flags->direct && fiber_flags->lazy);
    if (fiber && !isThreadFiber) {
      fiber->Delete();
    }
    isThreadFiber = true;
    fiber = newFiber;
    return this;
  }
  LazyFiberData *Init(unsigned flags, const char* file, int line) {
    assert(!fiber_flags->direct && fiber_flags->lazy);
    //    this->flags = flags;
    TsanHappensBeforeStore(&fiber);
    createFile = file;
    createLine = line;
    return this;
  }

  void Reset() {
    assert(!fiber_flags->direct && fiber_flags->lazy);
    //    flags = 0;
    name.clear();
    if (fiber != nullptr && !isThreadFiber) {
      fiber->Delete();
    }
    fiber = nullptr;
    createFile = nullptr;
    createLine = 0;
    switchFile = nullptr;
    switchLine = 0;
    isThreadFiber = false;
  }

  static LazyFiberData *New(unsigned flags, const char* file, int line) {
    return DataPoolEntry<LazyFiberData>::New()->Init(flags, file, line);
  }
  static LazyFiberData *New(FiberData *newFiber) {
    return DataPoolEntry<LazyFiberData>::New()->Init(newFiber);
  }

  LazyFiberData(DataPool<LazyFiberData> *dp)
      : DataPoolEntry<LazyFiberData>(dp) {}
  ~LazyFiberData() {
    Reset();
  }
};
__thread LazyFiberData *LazyFiberData::currentFiber{nullptr};


class LazyTsanFiberData;
typedef DataPool<LazyTsanFiberData> LazyTsanFiberDataPool;
template <>
__thread LazyTsanFiberDataPool *LazyTsanFiberDataPool::ThreadDataPool = nullptr;

/// Data structure to lazily wrap a TSan fiber.
class LazyTsanFiberData final : public DataPoolEntry<LazyTsanFiberData> {
private:
  unsigned flags{0};
  int marker{0};
  void *fiber{nullptr};
  bool isThreadFiber{false};
  std::string name{};
  const char *switchFile{nullptr}, *createFile{nullptr};
  int switchLine{0}, createLine{0};

public:
  static __thread LazyTsanFiberData *currentFiber;

  void SwitchToFiber(unsigned flags, const char* file, int line) override {
    assert(fiber_flags->direct && fiber_flags->lazy);
    assert(marker==0xdeadbeef);
    bool isNew{false};
    if (!fiber) {
      if (flags)
        TsanIgnoreSyncBegin();
      fiber = __tsan_create_fiber(flags);
      fibers++;
      if (flags)
        TsanIgnoreSyncEnd();
      if (!name.empty())
        __tsan_set_fiber_name(fiber, name.c_str());
      isNew = true;
    }
    __tsan_switch_to_fiber(fiber, flags);
    if (isNew && !this->flags)
      TsanHappensAfterStore(&fiber);
    currentFiber = this;
  }
  void SetFiberName(const char *name) override {
    assert(fiber_flags->direct && fiber_flags->lazy);
    assert(marker==0xdeadbeef);
    if (fiber)
      return __tsan_set_fiber_name(fiber, name);
    this->name = std::string{name};
  }
  int GetFiberStacksize() override {
    assert(fiber_flags->direct && fiber_flags->lazy);
    assert(marker==0xdeadbeef);
    assert (fiber);
    return __tsan_get_fiber_stacksize(fiber);
  }
  static LazyTsanFiberData *getCurrentFiber() {
    assert(fiber_flags->direct && fiber_flags->lazy);
    assert(currentFiber);
    assert(currentFiber->marker==0xdeadbeef);
    /*if (!currentFiber) {
      currentFiber = LazyTsanFiberData::New(__tsan_get_current_fiber());
    } else {
      assert(currentFiber->fiber == __tsan_get_current_fiber());
    }*/
    return currentFiber;
  }

  LazyTsanFiberData *Init(void* newFiber) {
    assert(fiber_flags->direct && fiber_flags->lazy);
    marker = 0xdeadbeef;
    assert(!fiber);
    /*if (fiber) {
      __tsan_destroy_fiber(fiber);
    }*/
    isThreadFiber = true;
    fiber = newFiber;
    return this;
  }
  LazyTsanFiberData *Init(unsigned flags, const char* file, int line) {
    assert(fiber_flags->direct && fiber_flags->lazy);
    marker = 0xdeadbeef;
    assert(!fiber);
    this->flags = flags;
    createFile = file;
    createLine = line;
    if (!flags)
      TsanHappensBeforeStore(&fiber);
    return this;
  }

  void Reset() {
    assert(fiber_flags->direct && fiber_flags->lazy);
    assert(marker==0xdeadbeef);
    //    flags = 0;
    name.clear();
    marker=0;
    if (fiber != nullptr) {
      if (!isThreadFiber)
        __tsan_destroy_fiber(fiber);
      fiber = nullptr;
      isThreadFiber = false;
    }
    createFile = nullptr;
    createLine = 0;
    switchFile = nullptr;
    switchLine = 0;
  }

  static LazyTsanFiberData *New(unsigned flags, const char* file, int line) {
    return DataPoolEntry<LazyTsanFiberData>::New()->Init(flags, file, line);
  }
  static LazyTsanFiberData *New(void *newFiber) {
    return DataPoolEntry<LazyTsanFiberData>::New()->Init(newFiber);
  }

  LazyTsanFiberData(DataPool<LazyTsanFiberData> *dp)
      : DataPoolEntry<LazyTsanFiberData>(dp) {}
  ~LazyTsanFiberData() {
    assert(!fiber || isThreadFiber);
  }
};
__thread LazyTsanFiberData *LazyTsanFiberData::currentFiber{nullptr};


#define TsanGetCurrentFiber() __fiber::__pool_get_current_fiber()
#define TsanCreateFiber(flag) __fiber::__pool_create_fiber_dbg(flag, __FILE__, __LINE__)
#define TsanDestroyFiber(fiber) __fiber::__pool_destroy_fiber(fiber)
#define TsanSwitchToFiber(fiber, flag)                                         \
  __fiber::__pool_switch_to_fiber_dbg(fiber, flag, __FILE__, __LINE__)
#define TsanSetFiberName(fiber, name)                                          \
  __fiber::__pool_set_fiber_name(fiber, name)
#define TsanGetFiberStacksize(fiber) __fiber::__pool_get_fiber_stacksize(fiber)
#define TsanFiberPoolInit __fiber::__pool_init
#define TsanFiberPoolFini __fiber::__pool_fini

void *__pool_get_current_fiber() {
  if (fiber_flags->direct) {
    if (fiber_flags->lazy) {
      if (!LazyTsanFiberData::currentFiber) {
        LazyTsanFiberData::currentFiber = LazyTsanFiberData::getCurrentFiber();
      }
      return LazyTsanFiberData::currentFiber;
    }
    return __tsan_get_current_fiber();
  }
  if (fiber_flags->lazy) {
    if (!LazyFiberData::currentFiber) {
      LazyFiberData::currentFiber = LazyFiberData::getCurrentFiber();
    }
    return LazyFiberData::currentFiber;
  }
  if (!FiberData::currentFiber) {
    FiberData::currentFiber = FiberData::getCurrentFiber();
  }
  return FiberData::currentFiber;
}
void *__pool_create_fiber_dbg(unsigned flags, const char* file, int line) {
  if (fiber_flags->direct) {
    if (fiber_flags->lazy) {
      return LazyTsanFiberData::New(flags, file, line);
    }
    if (flags)
      TsanIgnoreSyncBegin();
    auto ret = __tsan_create_fiber(flags);
    if (flags)
      TsanIgnoreSyncEnd();
    return ret;
  }
  if (fiber_flags->lazy) {
    return LazyFiberData::New(flags, file, line);
  }
  return FiberData::New(flags, file, line);
}
void *__pool_create_fiber(unsigned flags) {
  return __pool_create_fiber_dbg(flags, NULL, 0);
}
void __pool_destroy_fiber(void *fiber) {
  if (fiber_flags->direct && !fiber_flags->lazy) {
    return __tsan_destroy_fiber(fiber);
  }
  reinterpret_cast<FiberApi *>(fiber)->Delete();
}
void __pool_switch_to_fiber_dbg(void *fiber, unsigned flags, const char* file, int line) {
  if (fiber_flags->direct && !fiber_flags->lazy) {
    return __tsan_switch_to_fiber(fiber, flags);
  }
  reinterpret_cast<FiberApi *>(fiber)->SwitchToFiber(flags, file, line);
}
void __pool_switch_to_fiber(void *fiber, unsigned flags) {
  __pool_switch_to_fiber_dbg(fiber, flags, NULL, 0);
}
void __pool_set_fiber_name(void *fiber, const char *name) {
  if (fiber_flags->direct && !fiber_flags->lazy) {
    return __tsan_set_fiber_name(fiber, name);
  }
  reinterpret_cast<FiberApi *>(fiber)->SetFiberName(name);
}
int __pool_get_fiber_stacksize(void *fiber) {
  if (fiber_flags->direct && !fiber_flags->lazy) {
    return __tsan_get_fiber_stacksize(fiber);
  }
  return reinterpret_cast<FiberApi *>(fiber)->GetFiberStacksize();
}

#define findTsanFunction(f, fSig)                                              \
  do {                                                                         \
    if (NULL == (f = fSig dlsym(RTLD_DEFAULT, #f)))                            \
      printf("Unable to find TSan function " #f ".\n");                        \
  } while (0)

#define findTsanFunctionName(f, name, fSig) f = fSig dlsym(RTLD_DEFAULT, #name)

void tool_init() {
  pagesize = getpagesize();
  const char *options = getenv("FIBER_OPTIONS");
  fiber_flags = new FiberFlags(options);

  findTsanFunctionName(AnnotateHappensAfter, AnnotateHappensAfterStore,
                       (void (*)(const char *, int, const volatile void *)));
  if (!AnnotateHappensAfter) {
    if (fiber_flags->verbose)
      printf("Using fallback annotation: AnnotateHappensAfter\n");
    findTsanFunction(AnnotateHappensAfter,
                     (void (*)(const char *, int, const volatile void *)));
  }
  findTsanFunctionName(AnnotateHappensBefore, AnnotateHappensBeforeStore,
                       (void (*)(const char *, int, const volatile void *)));
  if (!AnnotateHappensBefore) {
    if (fiber_flags->verbose)
      printf("Using fallback annotation: AnnotateHappensBefore\n");
    findTsanFunction(AnnotateHappensBefore,
                     (void (*)(const char *, int, const volatile void *)));
  }
  findTsanFunction(AnnotateIgnoreSyncBegin, (void (*)(const char *, int)));
  findTsanFunction(AnnotateIgnoreSyncEnd, (void (*)(const char *, int)));
  findTsanFunction(__tsan_get_current_fiber, (void *(*)(void)));
  findTsanFunction(__tsan_create_fiber, (void *(*)(unsigned)));
  findTsanFunction(__tsan_destroy_fiber, (void (*)(void *)));
  findTsanFunction(__tsan_switch_to_fiber, (void (*)(void *, unsigned)));
  findTsanFunction(__tsan_set_fiber_name, (void (*)(void *, const char *)));
  findTsanFunction(__tsan_get_fiber_stacksize, (int (*)(void *)));
  if (fiber_flags->verbose) {
    if (fiber_flags->lazy) {
      if (fiber_flags->direct) {
        printf("Using Direct Lazy Fiberpool\n");
      } else {
        printf("Using Lazy Fiberpool\n");
      }
    } else {
      if (fiber_flags->direct) {
        printf("Using Direct Fiberpool\n");
      } else {
        printf("Using Eager Fiberpool\n");
      }
    }
  }
}

static std::once_flag onceFlag;
void __pool_init() {
  if (FiberDataPool::ThreadDataPool)
    return;
  std::call_once(onceFlag, tool_init);
  if (!fiber_flags->direct) {
    FiberDataPool::ThreadDataPool = new FiberDataPool;
  }
  if (fiber_flags->lazy) {
    if (fiber_flags->direct) {
      LazyTsanFiberDataPool::ThreadDataPool = new LazyTsanFiberDataPool;
      if (!LazyTsanFiberData::currentFiber) {
        LazyTsanFiberData::currentFiber = LazyTsanFiberData::New(__tsan_get_current_fiber());;
      }
    } else {
      LazyFiberDataPool::ThreadDataPool = new LazyFiberDataPool;
      if (!LazyFiberData::currentFiber) {
        LazyFiberData::currentFiber = LazyFiberData::New(FiberData::getCurrentFiber());
      }
    }
  } else if (!fiber_flags->direct) {
    if (!FiberData::currentFiber) {
      FiberData::currentFiber = FiberData::New(__tsan_get_current_fiber());
    }
  }
}
void __pool_fini() {
  if (fiber_flags->lazy)
    if (LazyFiberDataPool::ThreadDataPool)
      delete LazyFiberDataPool::ThreadDataPool;
  if (FiberDataPool::ThreadDataPool)
    delete FiberDataPool::ThreadDataPool;
}

} // namespace __fiber
