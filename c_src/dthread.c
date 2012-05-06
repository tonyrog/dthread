/*
 * Reusable? API to handle commands from an
 * Erlang drivers in a thread. This thread can be used for
 * blocking operations towards operating system, replies go
 * either through port to process or straight to calling process depending
 * on SMP is supported or not.
 */

#include "dthread.h"

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/******************************************************************************
 *
 *   Messages
 *
 *****************************************************************************/

dmessage_t* dmessage_alloc(size_t n)
{
    size_t sz = sizeof(dmessage_t) + n;
    dmessage_t* mp = driver_alloc(sz);
    if (mp) {
	memset(mp, 0, sz);
	mp->buffer = mp->data;
	mp->used = 0;
	mp->size = n;
    }
    return mp;
}

void dmessage_free(dmessage_t* mp)
{
    if (mp->release)
	(*mp->release)(mp->buffer, mp->size, mp->udata);
    else if (mp->buffer != mp->data)
	driver_free(mp->buffer);
    driver_free(mp);
}

// create a message with optional dynamic buffer
dmessage_t* dmessage_create_r(int cmd, 
			      void (*release)(char*, size_t, void*),
			      void* udata,
			      char* buf, size_t len)
{
    dmessage_t* mp;
    if (release) {
	if ((mp = dmessage_alloc(0))) {
	    mp->cmd = cmd;
	    mp->udata = udata;
	    mp->buffer = buf;
	    mp->size   = len;
	    mp->used = len;
	}
    }
    else {
	if ((mp = dmessage_alloc(len))) {
	    mp->cmd = cmd;
	    mp->udata = udata;
	    memcpy(mp->buffer, buf, len);
	    mp->used = len;
	}
    }
    return mp;
}

// simple version of dmessage_create_r
dmessage_t* dmessage_create(int cmd,char* buf, size_t len)
{
    return dmessage_create_r(cmd, NULL, NULL, buf, len);
}

// Peek at queue front
dmessage_t* dthread_mqueue_peek(dthread_t* thr)
{
    dmessage_t* mp;
    
    erl_drv_mutex_lock(thr->mq_mtx);
    mp = thr->mq_front;
    erl_drv_mutex_unlock(thr->mq_mtx);
    return mp;
}

// Get message from queue front
dmessage_t* dthread_mqueue_get(dthread_t* thr)
{
    dmessage_t* mp;

    erl_drv_mutex_lock(thr->mq_mtx);
    if ((mp = thr->mq_front) != NULL) {
	if (!(thr->mq_front = mp->next))
	    thr->mq_rear = NULL;
	thr->mq_len--;
    }
    erl_drv_mutex_unlock(thr->mq_mtx);
    return mp;
}

// Put message at queue rear
int dthread_mqueue_put(dthread_t* thr, dmessage_t* mp)
{
    dmessage_t* mr;

    erl_drv_mutex_lock(thr->mq_mtx);

    thr->mq_len++;
    mp->next = 0;
    if ((mr = thr->mq_rear) != NULL)
	mr->next = mp;
    else
	thr->mq_front = mp;
    thr->mq_rear = mp;

    erl_drv_mutex_unlock(thr->mq_mtx);

#ifdef __WIN32__
    SetEvent(HD(thr->mq_signal[1]), TRUE);
    return 1;
#else
    return write(FD(thr->mq_signal[1]), "W", 1);
#endif
}


int dthread_send(dthread_t* destination, dthread_t* source,
		 dmessage_t* mp)
{
    mp->source = source;
    return dthread_mqueue_put(destination, mp);
}


int dthread_command(dthread_t* destination, dthread_t* source,
		    int cmd, char* buf, int len)
{
    dmessage_t* mp;

    if (!(mp = dmessage_create(cmd, buf, len)))
	return -1;
    mp->from = source->caller;
    mp->ref   = ++source->ref;
    return dthread_send(destination, source, mp);
}


dmessage_t* dmessage_recv(dthread_t* thr, dthread_t** source)
{
    dmessage_t* mp;
    if (!(mp = dthread_mqueue_get(thr)))
	return NULL;
    if (source)
	*source = mp->source;
#ifdef WIN32
    ResetEvent(HD(thr->mq_signal[0]));
#else
    {
	char buf[1];
	read(FD(thr->mq_signal[0]), buf, 1);
    }
#endif
    return mp;
}

int dthread_send_term(dthread_t* thr, dthread_t* source, 
		      ErlDrvTermData target,
		      ErlDrvTermData* spec, int len)
{
    if (thr->smp_support)
	return driver_send_term(thr->port, target, spec, len);
    else {
	dmessage_t* mp;
	mp = dmessage_create(DTHREAD_CMD_SEND_TERM,(char*)spec,
			     len*sizeof(ErlDrvTermData));
	mp->to = target;
	return dthread_send(thr, source, mp);
    }
}

int dthread_output_term(dthread_t* thr, dthread_t* source, 
			ErlDrvTermData* spec, int len)
{
    return dthread_send_term(thr, source, thr->owner, spec, len);
}

// release buffer copy when not used any more
static void release_spec_3(char* buf, size_t used, void* udata)
{
    ErlDrvTermData* spec = (ErlDrvTermData*)buf;
    driver_free((void*)spec[3]);
}


int dthread_output(dthread_t* thr, dthread_t* source, 
		    char* buf, int len)
{
    // generate {Port, {data, Data}}
    ErlDrvTermData spec[9];
    
    spec[0] = thr->dport;
    spec[1] = driver_mk_atom("port");
    spec[2] = ERL_DRV_BUF2BINARY;
    spec[3] = (ErlDrvTermData) buf;
    spec[4] = (ErlDrvTermData) len;
    spec[5] = ERL_DRV_TUPLE;
    spec[6] = 2;
    spec[7] = ERL_DRV_TUPLE;
    spec[8] = 2;

    if (thr->smp_support)
	return driver_send_term(thr->port, thr->owner, spec, 9);
    else {
	dmessage_t* mp;
	char* buf_copy;

	// make a copy 
	buf_copy = driver_alloc(len);
	memcpy(buf_copy, buf, len);
	spec[3] = (ErlDrvTermData) buf_copy;

	mp = dmessage_create(DTHREAD_CMD_SEND_TERM,(char*)spec,
			     9*sizeof(ErlDrvTermData));
	mp->release = release_spec_3;
	mp->to = thr->owner;
	return dthread_send(thr, source, mp);
    }
}

void dthread_finish(dthread_t* thr)
{
    dmessage_t* mp;

    if (thr->mq_mtx) {
	erl_drv_mutex_destroy(thr->mq_mtx);
	thr->mq_mtx = NULL;
    }
    mp = thr->mq_front;
    while(mp) {
	dmessage_t* mn = mp->next;
	dmessage_free(mp);
	mp = mn;
    }
    thr->mq_front = thr->mq_rear = NULL;
#ifdef WIN32
    if (HD(thr->mq_signal[0]) != INVALID_HANDLE) {
	driver_select(thr->port, thr->mq_signal[0], ERL_DRV_USE, 0);
	thr->mq_signal[0] = INVALID_HANDLE;
    }
#else
    if (FD(thr->mq_signal[0]) >= 0) {
	driver_select(thr->port, thr->mq_signal[0], ERL_DRV_USE, 0);
	thr->mq_signal[0] = (ErlDrvEvent) ((long)-1);
    }
    if (FD(thr->mq_signal[1]) >= 0) {
	driver_select(thr->port, thr->mq_signal[1], ERL_DRV_USE, 0);
	thr->mq_signal[1] =  (ErlDrvEvent) ((long)-1);
    }
#endif
}


int dthread_init(dthread_t* thr, ErlDrvPort port)
{
    ErlDrvSysInfo sys_info;

    memset(thr, 0, sizeof(dthread_t));
#ifdef __WIN32__
    thr->mq_signal[0] = (ErlDrvEvent)INVALID_HANDLE;
    thr->mq_signal[1] = (ErlDrvEvent)INVALID_HANDLE;
#else
    thr->mq_signal[0] = (ErlDrvEvent) ((long)-1);
    thr->mq_signal[1] = (ErlDrvEvent) ((long)-1);
#endif

    driver_system_info(&sys_info, sizeof(ErlDrvSysInfo));
    // smp_support is used for message passing from thread to
    // calling process. if SMP is supported the message will go
    // directly to sender, otherwise it must be sent to port 
    thr->smp_support = sys_info.smp_support;
    thr->port = port;
    thr->dport = driver_mk_port(port);

    if (!(thr->mq_mtx = erl_drv_mutex_create("queue_mtx")))
	return -1;
#ifdef __WIN32__
    if (!(thr->mq_signal[0] = CreateEvent(NULL, TRUE, FALSE, NULL))) {
	dthread_finish(thr);
	return -1;
    }
    driver_select(thr->port,thr->mq_signal[0],ERL_DRV_USE,1);
#else
    {
	int pfd[2];
	if (pipe(pfd) < 0) {
	    dthread_finish(thr);
	    return -1;
	}
	thr->mq_signal[0] = (ErlDrvEvent) ((long)pfd[0]);
	thr->mq_signal[1] = (ErlDrvEvent) ((long)pfd[1]);
	driver_select(thr->port,thr->mq_signal[1],ERL_DRV_USE,1);
	driver_select(thr->port,thr->mq_signal[0],ERL_DRV_READ|ERL_DRV_USE,1);
    }
#endif
    return 0;
}



dthread_t* dthread_start(ErlDrvPort port,
			 void* (*func)(void* arg),
			 void* arg, int stack_size)
{
    ErlDrvThreadOpts* opts = NULL;
    dthread_t* thr = NULL;

    if (!(thr = driver_alloc(sizeof(dthread_t))))
	return 0;

    if (dthread_init(thr, port) < 0)
	goto error;

    if (!(opts = erl_drv_thread_opts_create("dthread_opts")))
	goto error;

    opts->suggested_stack_size = stack_size;
    thr->arg = arg;

    if (erl_drv_thread_create("dthread", &thr->tid, func, thr, opts) < 0)
	goto error;
    erl_drv_thread_opts_destroy(opts);
    return thr;

error:
    dthread_finish(thr);
    if (opts)
        erl_drv_thread_opts_destroy(opts);
    dthread_finish(thr);
    driver_free(thr);
    return 0;
}

int dthread_stop(dthread_t* target, dthread_t* source, 
		 void** exit_value)
{
    dmessage_t* mp;
    int r;

    if (!(mp = dmessage_create(DTHREAD_CMD_STOP, NULL, 0)))
	return -1;
    dthread_send(target, source, mp);

    r = erl_drv_thread_join(target->tid, exit_value);
    DEBUGF("erl_drv_thread_join: return=%d, exit_value=%p", r, *exit_value);

    dthread_finish(target);

    driver_free(target);
    return 0;
}

void dthread_exit(void* value)
{
    erl_drv_thread_exit(value);
}
