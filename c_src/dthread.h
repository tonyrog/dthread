#ifndef __DTHREAD_H__
#define __DTHREAD_H__

struct _dthread_t;

#include "erl_driver.h"

#ifdef DEBUG
extern void emit_error(char* file, int line, ...);
#define DEBUGF(args...) emit_error(__FILE__,__LINE__,args)
#else
#define DEBUGF(args...)
#endif

#ifdef __WIN32__
#define HD(e) ((Handle)(e))
#else
#define FD(e) ((int)((long)(e)))
#endif


// Builtin commands are negative, user command must > 0

#define DTHREAD_CMD_STOP          -1
#define DTHREAD_CMD_SEND_TERM     -2
#define DTHREAD_CMD_OUTPUT_TERM   -3
#define DTHREAD_CMD_OUTPUT        -4

typedef struct _dmessage_t
{
    struct _dmessage_t*  next;  // next message in queue
    int                   cmd;  // same as ctl 
    struct _dthread_t* source;  // sender thread
    ErlDrvTermData       from;  // sender pid
    ErlDrvTermData        to;   // receiver pid (if any)
    ErlDrvTermData        ref;  // sender ref
    void* udata;                // user data
    void (*release)(char*, size_t, void*);  // buffer release function
    size_t size;          // total allocated size of buffer
    size_t used;          // total used part of buffer
    char*  buffer;        // points to data or allocated
    char   data[0];
} dmessage_t;

typedef struct _dthread_t {
    ErlDrvTid      tid;         // thread id
    void*          arg;         // thread init argument
    ErlDrvPort     port;        // port controling the thread
    ErlDrvTermData dport;       // the port identifier as DriverTermData
    ErlDrvTermData owner;       // owner process pid 
    ErlDrvTermData caller;      // last caller (driver_caller)
    ErlDrvTermData    ref;      // last sender ref
    int       smp_support;      // SMP support or not
    // Message queue
    ErlDrvMutex*   mq_mtx;       // message queue lock
    int            mq_len;       // message queue length
    dmessage_t*    mq_front;     // pick from front
    dmessage_t*    mq_rear;      // insert at rear
    ErlDrvEvent    mq_signal[2]; // event signaled when items is enqueued
} dthread_t;

extern dmessage_t* dmessage_alloc(size_t n);
extern void dmessage_free(dmessage_t* mp);
extern dmessage_t* dmessage_create_r(int cmd, 
				     void (*release)(char*, size_t, void*),
				     void* udata,
				     char* buf, size_t len);
extern dmessage_t* dmessage_create(int cmd,char* buf, size_t len);

extern int         dthread_mqueue_put(dthread_t* thr, dmessage_t* m);
extern dmessage_t* dthread_mqueue_get(dthread_t* thr);
extern dmessage_t* dthread_mqueue_peek(dthread_t* thr);

extern int dthread_command(dthread_t* thr, dthread_t* source,
			   int cmd, char* buf, int len);
extern int dthread_send(dthread_t* thr, dthread_t* source,
			dmessage_t* mp);
extern int dthread_send_term(dthread_t* thr, dthread_t* source,
			     ErlDrvTermData target,
			     ErlDrvTermData* spec, int len);
extern int dthread_output_term(dthread_t* thr, dthread_t* source,
			       ErlDrvTermData* spec, int len);
extern int dthread_output(dthread_t* thr, dthread_t* source,
			  char* buf, int len);



extern dmessage_t* dthread_recv(dthread_t* self, dthread_t** source);

extern int dthread_init(dthread_t* thr, ErlDrvPort port);
extern void dthread_finish(dthread_t* thr);
extern dthread_t* dthread_start(ErlDrvPort port,
				void* (*func)(void* arg),
				void* arg, int stack_size);
extern int dthread_stop(dthread_t* target, dthread_t* source, 
			void** exit_value);
extern void dthread_exit(void* value);

#endif
