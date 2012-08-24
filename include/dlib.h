#ifndef __DLIB_H__
#define __DLIB_H__

#include "erl_driver.h"

extern void dlib_init(void);
extern void dlib_finish(void);

extern void dlib_emit_error(int level, char* file, int line, ...);
extern void dlib_set_debug(int level);

extern void* dlib_alloc(size_t sz, char* file, int line);
extern void* dlib_zalloc(size_t sz, char* file, int line);
extern void  dlib_free(void* ptr, char* file, int line);
extern void* dlib_realloc(void* ptr, size_t sz, char* file, int line);
extern void  dlib_zero(void* ptr, size_t sz, char* file, int line);
extern size_t dlib_allocated(void);

#define DLOG_DEBUG     7
#define DLOG_INFO      6
#define DLOG_NOTICE    5
#define DLOG_WARNING   4
#define DLOG_ERROR     3
#define DLOG_CRITICAL  2
#define DLOG_ALERT     1
#define DLOG_EMERGENCY 0
#define DLOG_NONE     -1

#ifdef DEBUG
#define DEBUGF(args...) dlib_emit_error(DLOG_DEBUG,__FILE__,__LINE__,args)
#define INFOF(args...)  dlib_emit_error(DLOG_INFO,__FILE__,__LINE__,args)
#else
#define DEBUGF(args...)
#define INFOF(args...)
#endif

#ifdef DEBUG_MEM
#define DALLOC(sz)        dlib_alloc((sz),__FILE__,__LINE__)
#define DZALLOC(sz)       dlib_zalloc((sz),__FILE__,__LINE__)
#define DFREE(ptr)        dlib_free((ptr),__FILE__,__LINE__)
#define DREALLOC(ptr,sz)  dlib_realloc((ptr),(sz),__FILE__,__LINE__)
#define DZERO(ptr,sz)     dlib_zero((ptr),(sz),__FILE__,__LINE__)
#else
#define DALLOC(sz)        driver_alloc((sz))
#define DZALLOC(sz)       driver_zalloc((sz))
#define DFREE(ptr)        driver_free((ptr))
#define DREALLOC(ptr,sz)  driver_realloc((ptr),(sz))
#define DZERO(ptr,sz)     memset((ptr),'\0',(sz))

static inline void* driver_zalloc(size_t sz)
{
    void *ptr = driver_alloc(sz);
    if (ptr != NULL)
	memset(ptr, '\0', sz);
    return ptr;
}

#endif

#endif
