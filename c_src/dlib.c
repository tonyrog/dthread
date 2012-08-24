#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <memory.h>

#include "../include/dlib.h"

static int    dlib_debug_level   = DLOG_NONE;
static size_t dlib_num_allocated = 0;

static ErlDrvMutex* dlib_mtx;

void dlib_init()
{
    dlib_debug_level   = DLOG_NONE;
    dlib_num_allocated = 0;
    dlib_mtx = erl_drv_mutex_create("dterm_mtx");
}

void dlib_finish()
{
    erl_drv_mutex_destroy(dlib_mtx);
}

//
// dlib  utils (move to separate file?)
//
size_t dlib_allocated(void)
{
    return dlib_num_allocated;
}

// allocation utilities 
void* dlib_alloc(size_t sz, char* file, int line)
{
    size_t* sptr;
    if ((sptr = driver_alloc(sizeof(size_t)+sz)) == NULL) {
	dlib_emit_error(DLOG_ALERT, file, line, "allocation failed");
    }
    else {
	*sptr++ = sz;
	dlib_num_allocated += sz;
    }
    return (void*) sptr;
}

void* dlib_zalloc(size_t sz, char* file, int line)
{
    size_t* sptr;
    if ((sptr = driver_alloc(sizeof(size_t)+sz)) == NULL) {
	dlib_emit_error(DLOG_ALERT, file, line, "allocation failed");
    }
    else {
	*sptr++ = sz;
	memset(sptr, '\0', sz);
	dlib_num_allocated += sz;
    }
    return (void*) sptr;
}

void dlib_free(void* ptr, char* file, int line)
{
    size_t* sptr = (size_t*) ptr;
    if (sptr) {
	size_t sz0 = *--sptr;
	if (sz0 > dlib_num_allocated) {
	    dlib_emit_error(DLOG_EMERGENCY, file, line,
			       "free more data than allocated");
	    exit(1);
	}
	dlib_num_allocated -= sz0;
	driver_free(sptr);
    }
}

void* dlib_realloc(void* ptr, size_t sz, char* file, int line)
{
    size_t* sptr = (size_t*) ptr;
    if (sptr) {
	size_t sz0 = *--sptr;
	if (sz0 > dlib_num_allocated) {
	    dlib_emit_error(DLOG_EMERGENCY, file, line,
			       "realloc more data than allocated");
	    exit(1);
	}
	dlib_num_allocated -= sz0;
	
    }
    if ((sptr = driver_realloc(sptr, sz+sizeof(size_t))) == NULL)
	dlib_emit_error(DLOG_ALERT, file, line, "reallocation failed");
    else {
	*sptr++ = sz;
	dlib_num_allocated += sz;
    }
    return sptr;
}

void dlib_zero(void* ptr, size_t sz, char* file, int line)
{
    size_t* sptr = (size_t*) ptr;
    if (sptr) {
	size_t sz0 = *(sptr-1);
	if (sz > sz0) {
	    dlib_emit_error(DLOG_EMERGENCY, file, line, "overwrite heap");
	    exit(1);
	}
	memset(ptr, '\0', sz);
    }
}

size_t dtherad_allocated(void)
{
    return dlib_num_allocated;
}

void dlib_set_debug(int level)
{
    dlib_debug_level = level;
}

void dlib_emit_error(int level, char* file, int line, ...)
{
    va_list ap;
    char* fmt;

    if ((dlib_debug_level >= 0) && (level <= dlib_debug_level)) {
	va_start(ap, line);
	fmt = va_arg(ap, char*);
	fprintf(stderr, "%s:%d: ", file, line); 
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\r\n");
	va_end(ap);
    }
}
