//
// Test driver using dthread library
//

#include <stdio.h>
#ifdef __WIN32__
#include "windows.h"
#define EAGAIN       ERROR_IO_PENDING
#define EWOULDBLOCK  ERROR_IO_PENDING
#define ENOMEM       ERROR_NOT_ENOUGH_MEMORY
#define EINVAL       ERROR_BAD_ARGUMENTS
#define EBUSY        ERROR_BUSY
#define EOVERFLOW    ERROR_TOO_MANY_CMDS
#define EMSGSIZE     ERROR_NO_DATA
#define ENOTCONN     ERROR_PIPE_NOT_CONNECTED
#define EINTR        ERROR_INVALID_AT_INTERRUPT_TIME //dummy
#else
#include <unistd.h>
#include <sys/select.h>
#endif

#include "erl_driver.h"
#include "dthread.h"

#include <ctype.h>
#include <stdint.h>
#include <memory.h>

// Hack to handle R15 driver used with pre R15 driver
#if ERL_DRV_EXTENDED_MAJOR_VERSION == 1
typedef int  ErlDrvSizeT;
typedef int  ErlDrvSSizeT;
#endif

#define DRV_OK       0
#define DRV_ERROR    1

typedef struct _ctx_t
{
    ErlDrvTermData  caller;     // recipient of sync reply

    dthread_t self;             // me
    dthread_t* other;           // the thread
} ctx_t;

ErlDrvEntry dthread_drv_entry;

#ifdef DEBUG
void emit_error(char* file, int line, ...)
{
    va_list ap;
    char* fmt;

    va_start(ap, line);
    fmt = va_arg(ap, char*);

    fprintf(stderr, "%s:%d: ", file, line); 
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\r\n");
    va_end(ap);
}
#endif

static inline void put_uint32(uint8_t* ptr, uint32_t v)
{
    ptr[0] = v>>24;
    ptr[1] = v>>16;
    ptr[2] = v>>8;
    ptr[3] = v;
}


static inline void put_uint16(uint8_t* ptr, uint16_t v)
{
    ptr[0] = v>>8;
    ptr[1] = v;
}

/* general control reply function */
static ErlDrvSSizeT ctl_reply(int rep, char* buf, ErlDrvSizeT len,
			      char** rbuf, ErlDrvSizeT rsize)
{
    char* ptr;

    if ((len+1) > rsize) {
	ptr = driver_alloc(len+1);
	*rbuf = ptr;
    }
    else
	ptr = *rbuf;
    *ptr++ = rep;
    memcpy(ptr, buf, len);
    return len+1;
}

/* general control error reply function */
#if 0
static ErlDrvSSizeT ctl_error(int err, char** rbuf, ErlDrvSizeT rsize)
{
    char response[256];		/* Response buffer. */
    char* s;
    char* t;

    for (s = erl_errno_id(err), t = response; *s; s++, t++)
	*t = tolower(*s);
    return ctl_reply(DRV_ERROR, response, t-response, rbuf, rsize);
}
#endif

#if 0
static ErlDrvTermData error_atom(int err)
{
    char errstr[256];
    char* s;
    char* t;

    for (s = erl_errno_id(err), t = errstr; *s; s++, t++)
	*t = tolower(*s);
    *t = '\0';
    return driver_mk_atom(errstr);
}
#endif

//
// Main thread function
//
void* dthread_dispatch(void* arg)
{
    dthread_t* self = (dthread_t*) arg;
    fd_set iset0;
    int qfd;

    DEBUGF("dthread_drv: dthread_dispatch started");
    
    FD_ZERO(&iset0);
    qfd = FD(self->mq_signal[0]);
    FD_SET(qfd, &iset0);

    while(1) {
	fd_set iset;
	int r;
	int nfds;

	FD_COPY(&iset0, &iset);
	nfds = qfd+1;
	r = select(nfds, &iset, NULL, NULL, NULL);
	if (r < 0) {
	    DEBUGF("dthread_drv: dthread_dispatch select failed=%d", r);
	    continue;
	}
	else if (r == 0) {
	    DEBUGF("dthread_drv: dthread_dispatch timeout");
	    continue;
	}
	if (FD_ISSET(qfd, &iset)) {
	    dmessage_t* mp = dthread_recv(self, NULL);

	    switch(mp->cmd) {
	    case DTHREAD_CMD_STOP:
		DEBUGF("dthread_drv: dthread_dispatch STOP");
		dmessage_free(mp);
		dthread_exit(0);
		break;
	    case 1:
		DEBUGF("dthread_drv: dthread_dispatch cmd=1");
		dthread_output(mp->source, self, "HELLO WORLD", 11);
		break;
	    case 2: {
		DEBUGF("dthread_drv: dthread_dispatch cmd=2");
		ErlDrvTermData spec[5];
		spec[0] = driver_mk_atom("x");
		spec[1] = driver_mk_atom("y");
		spec[2] = driver_mk_atom("z");
		spec[3] = ERL_DRV_TUPLE;
		spec[4] = 3;
		dthread_output_term(mp->source, self, spec, 5);
		break;
	    }
	    default:
		DEBUGF("dthread_drv: dthread_dispatch cmd=%d", mp->cmd);
		break;
	    }
	    dmessage_free(mp);
	}
    }    
}


// setup global object area
// load atoms etc.

static int dthread_drv_init(void)
{
    DEBUGF("dthread_drv: driver init");
    return 0;
}

// clean up global settings
static void dthread_drv_finish(void)
{
    DEBUGF("dthread_drv: finish");
}

static ErlDrvData dthread_drv_start(ErlDrvPort port, char* command)
{
    ctx_t* ctx;

    DEBUGF("dthread_drv: start");

    ctx = driver_alloc(sizeof(ctx_t));
    
    dthread_init(&ctx->self, port);

    ctx->other = dthread_start(port, dthread_dispatch, ctx, 1024);

    set_port_control_flags(port, PORT_CONTROL_FLAG_BINARY);

    return (ErlDrvData) ctx;
}


static void dthread_drv_stop(ErlDrvData d)
{
    ctx_t* ctx = (ctx_t*) d;
    void* value;

    DEBUGF("dthread_drv: stop");
    dthread_stop(ctx->other, &ctx->self, &value);
    dthread_finish(&ctx->self);
    driver_free(ctx);
}

static ErlDrvSSizeT dthread_drv_ctl(ErlDrvData d, unsigned int cmd,
				    char* buf, ErlDrvSizeT len,
				    char** rbuf, ErlDrvSizeT rsize)
{
    ctx_t* ctx = (ctx_t*) d;
    char ref_buf[sizeof(uint32_t)];

    DEBUGF("dthread_drv: ctl: cmd=%u, len=%d", cmd, len);

    ctx->self.caller = driver_caller(ctx->self.port);
    dthread_command(ctx->other, &ctx->self, cmd, buf, len);

    put_uint32((unsigned char*)ref_buf, (uint32_t) ctx->self.ref);
    return ctl_reply(DRV_OK, ref_buf, sizeof(ref_buf), rbuf, rsize);
}

static void dthread_drv_output(ErlDrvData d, char* buf, ErlDrvSizeT len)
{
    (void) d;
    (void) buf;
    (void) len;
    // ctx_t*   ctx = (ctx_t*) d;

    DEBUGF("dthread_drv: output");

    // ctx->self.caller = driver_caller(ctx->self.port);
    // dthread_command(ctx->other, &ctx->self, CMD_SEND, buf, len);    
}

static void dthread_drv_timeout(ErlDrvData d)
{
    (void) d;
    // ctx_t* ctx = (ctx_t*) d;
    DEBUGF("dthread_drv: output");
}

static void dthread_drv_ready_input(ErlDrvData d, ErlDrvEvent e)
{
    ctx_t* ctx = (ctx_t*) d;

    DEBUGF("dthread_drv: ready_input");
    if (ctx->self.mq_signal[0] == e) { // got input !
	dmessage_t* mp = dthread_recv(&ctx->self, NULL);

	switch(mp->cmd) {
	case DTHREAD_CMD_OUTPUT_TERM:
	    driver_output_term(ctx->self.port, (ErlDrvTermData*) mp->buffer,
			       mp->used / sizeof(ErlDrvTermData));
	    break;
	case DTHREAD_CMD_SEND_TERM:
	    driver_send_term(ctx->self.port, mp->from, /* orignal from ! */
			     (ErlDrvTermData*) mp->buffer,
			     mp->used / sizeof(ErlDrvTermData)); 
	    break;
	case DTHREAD_CMD_OUTPUT:
	    driver_output(ctx->self.port, mp->buffer, mp->used);
	    break;
	default:
	    DEBUGF("dthread_drv: read_input cmd=%d not matched",
		   mp->cmd);
	    break;
	}
	dmessage_free(mp);
    }
}

static void dthread_drv_ready_output(ErlDrvData d, ErlDrvEvent e)
{
    DEBUGF("dthread_drv: read_output");
}

static void dthread_drv_stop_select(ErlDrvEvent event, void* arg)
{
    (void) arg;
    DEBUGF("dthread_drv: stop_select");
#ifdef __WIN32__
    CloseHandle(HD(event));
#else
    close(FD(event));
#endif
}


DRIVER_INIT(dthread_drv)
{
    ErlDrvEntry* ptr = &dthread_drv_entry;

    DEBUGF("DRIVER_INIT");

    memset(ptr, 0, sizeof(ErlDrvEntry));

    ptr->init  = dthread_drv_init;
    ptr->start = dthread_drv_start;
    ptr->stop  = dthread_drv_stop;
    ptr->output = dthread_drv_output;
    ptr->ready_input  = dthread_drv_ready_input;
    ptr->ready_output = dthread_drv_ready_output;
    ptr->finish = dthread_drv_finish;
    ptr->driver_name = "dthread_drv";
    ptr->control = dthread_drv_ctl;
    ptr->timeout = dthread_drv_timeout;
    ptr->extended_marker = ERL_DRV_EXTENDED_MARKER;
    ptr->major_version = ERL_DRV_EXTENDED_MAJOR_VERSION;
    ptr->minor_version = ERL_DRV_EXTENDED_MINOR_VERSION;
    ptr->driver_flags = ERL_DRV_FLAG_USE_PORT_LOCKING;
    ptr->process_exit = 0;
    ptr->stop_select = dthread_drv_stop_select;
    return ptr;
}
