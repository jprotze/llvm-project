namespace __fiber {

#define TsanHappensBeforeStore(cv) AnnotateHappensBefore(__FILE__, __LINE__, cv)
#define TsanHappensAfterStore(cv) AnnotateHappensAfter(__FILE__, __LINE__, cv)
#define TsanIgnoreSyncBegin() AnnotateIgnoreSyncBegin(__FILE__, __LINE__)
#define TsanIgnoreSyncEnd() AnnotateIgnoreSyncEnd(__FILE__, __LINE__)

#define TsanGetCurrentFiber() __pool_get_current_fiber()
#define TsanCreateFiber(flag) __pool_create_fiber_dbg(flag, __FILE__, __LINE__)
#define TsanDestroyFiber(fiber) __pool_destroy_fiber(fiber)
#define TsanSwitchToFiber(fiber, flag)                                         \
  __pool_switch_to_fiber_dbg(fiber, flag, __FILE__, __LINE__)
#define TsanSetFiberName(fiber, name)                                          \
  __pool_set_fiber_name(fiber, name)
#define TsanGetFiberStacksize(fiber) __pool_get_fiber_stacksize(fiber)
#define TsanFiberPoolInit __pool_init
#define TsanFiberPoolFini __pool_fini

} // namespace __fiber


extern "C" void *__pool_get_current_fiber() ;

extern "C" void *__pool_create_fiber_dbg(unsigned flags, const char* file, int line) ;

extern "C" void *__pool_create_fiber(unsigned flags) ;

extern "C" void __pool_destroy_fiber(void *fiber) ;

extern "C" void __pool_switch_to_fiber_dbg(void *fiber, unsigned flags, const char* file, int line) ; 

extern "C" void __pool_switch_to_fiber(void *fiber, unsigned flags) ;

extern "C"  void __pool_set_fiber_name(void *fiber, const char *name) ;

extern "C" int __pool_get_fiber_stacksize(void *fiber) ;

extern "C" void __pool_init();

extern "C" void __pool_fini();