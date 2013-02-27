/****** BEGIN COPYRIGHT *******************************************************
 *
 * Copyright (C) 2007 - 2012, Rogvall Invest AB, <tony@rogvall.se>
 *
 * This software is licensed as described in the file COPYRIGHT, which
 * you should have received as part of this distribution. The terms
 * are also available at http://www.rogvall.se/docs/copyright.txt.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYRIGHT file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****** END COPYRIGHT ********************************************************/
#ifndef __DLIB_H__
#define __DLIB_H__

#include "erl_driver.h"

extern void dlib_init(void);
extern void dlib_finish(void);

extern void dlib_emit_error(int level, char* file, int line, ...);
extern void dlib_set_debug(int level);

extern int dlib_debug_level;

extern void* dlib_alloc(size_t sz, char* file, int line);
extern void* dlib_zalloc(size_t sz, char* file, int line);
extern void  dlib_free(void* ptr, char* file, int line);
extern void* dlib_realloc(void* ptr, size_t sz, char* file, int line);
extern void  dlib_zero(void* ptr, size_t sz, char* file, int line);
extern size_t dlib_allocated(void);
extern size_t dlib_total_allocated(void);

#define DLOG_DEBUG     7
#define DLOG_INFO      6
#define DLOG_NOTICE    5
#define DLOG_WARNING   4
#define DLOG_ERROR     3
#define DLOG_CRITICAL  2
#define DLOG_ALERT     1
#define DLOG_EMERGENCY 0
#define DLOG_NONE     -1

#define DLOG(level,file,line,args...) do { \
	if (((level) == DLOG_EMERGENCY) ||				\
	    ((dlib_debug_level >= 0) && ((level) <= dlib_debug_level))) { \
	    dlib_emit_error((level),(file),(line),args);		\
	}								\
    } while(0)
	
#define DEBUGF(args...) DLOG(DLOG_DEBUG,__FILE__,__LINE__,args)
#define INFOF(args...)  DLOG(DLOG_INFO,__FILE__,__LINE__,args)
#define NOTICEF(args...)  DLOG(DLOG_NOTICE,__FILE__,__LINE__,args)
#define WARNINGF(args...)  DLOG(DLOG_WARNING,__FILE__,__LINE__,args)
#define ERRORF(args...)  DLOG(DLOG_ERROR,__FILE__,__LINE__,args)
#define CRITICALF(args...)  DLOG(DLOG_CRITICAL,__FILE__,__LINE__,args)
#define ALERTF(args...)  DLOG(DLOG_ALERT,__FILE__,__LINE__,args)
#define EMERGENCYF(args...)  DLOG(DLOG_EMERGENCY,__FILE__,__LINE__,args)

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
