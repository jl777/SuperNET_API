/*
    Copyright (c) 2013 Martin Sustrik  All rights reserved.
    Copyright (c) 2013 GoPivotal, Inc.  All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include "../utils/alloc.h"
#include "../utils/closefd.h"
#include "../utils/cont.h"
#include "../utils/fast.h"
#include "../utils/err.h"
#include "../utils/attr.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef __PNACL
#include <sys/uio.h>
#else
#include <glibc-compat/sys/uio.h>
#endif

#define NN_USOCK_STATE_IDLE 1
#define NN_USOCK_STATE_STARTING 2
#define NN_USOCK_STATE_BEING_ACCEPTED 3
#define NN_USOCK_STATE_ACCEPTED 4
#define NN_USOCK_STATE_CONNECTING 5
#define NN_USOCK_STATE_ACTIVE 6
#define NN_USOCK_STATE_REMOVING_FD 7
#define NN_USOCK_STATE_DONE 8
#define NN_USOCK_STATE_LISTENING 9
#define NN_USOCK_STATE_ACCEPTING 10
#define NN_USOCK_STATE_CANCELLING 11
#define NN_USOCK_STATE_STOPPING 12
#define NN_USOCK_STATE_STOPPING_ACCEPT 13
#define NN_USOCK_STATE_ACCEPTING_ERROR 14

#define NN_USOCK_ACTION_ACCEPT 1
#define NN_USOCK_ACTION_BEING_ACCEPTED 2
#define NN_USOCK_ACTION_CANCEL 3
#define NN_USOCK_ACTION_LISTEN 4
#define NN_USOCK_ACTION_CONNECT 5
#define NN_USOCK_ACTION_ACTIVATE 6
#define NN_USOCK_ACTION_DONE 7
#define NN_USOCK_ACTION_ERROR 8
#define NN_USOCK_ACTION_STARTED 9

#define NN_USOCK_SRC_FD 1
#define NN_USOCK_SRC_TASK_CONNECTING 2
#define NN_USOCK_SRC_TASK_CONNECTED 3
#define NN_USOCK_SRC_TASK_ACCEPT 4
#define NN_USOCK_SRC_TASK_SEND 5
#define NN_USOCK_SRC_TASK_RECV 6
#define NN_USOCK_SRC_TASK_STOP 7

/*  Private functions. */
static void nn_usock_init_from_fd (struct nn_usock *self,int32_t s);
static int nn_usock_send_raw (struct nn_usock *self,struct msghdr *hdr);
static int nn_usock_recv_raw (struct nn_usock *self,void *buf,size_t *len);
static int nn_usock_geterr (struct nn_usock *self);
static void nn_usock_handler (struct nn_fsm *self,int32_t src,int32_t type,void *srcptr);
static void nn_usock_shutdown (struct nn_fsm *self,int32_t src,int32_t type,void *srcptr);

void nn_usock_init(struct nn_usock *self,int32_t src,struct nn_fsm *owner)
{
    // Initalise the state machine
    nn_fsm_init(&self->fsm, nn_usock_handler,nn_usock_shutdown,src,self,owner);
    self->state = NN_USOCK_STATE_IDLE;
    // Choose a worker thread to handle this socket
    self->worker = nn_fsm_choose_worker(&self->fsm);
    // Actual file descriptor will be generated during 'start' step
    self->s = -1;
    self->errnum = 0;
    self->in.buf = NULL;
    self->in.len = 0;
    self->in.batch = NULL;
    self->in.batch_len = 0;
    self->in.batch_pos = 0;
    self->in.pfd = NULL;
    memset(&self->out.hdr,0,sizeof(struct msghdr));
    // Initialise tasks for the worker thread
    nn_worker_fd_init(&self->wfd,NN_USOCK_SRC_FD,&self->fsm);
    nn_worker_task_init(&self->task_connecting,NN_USOCK_SRC_TASK_CONNECTING,&self->fsm);
    nn_worker_task_init(&self->task_connected,NN_USOCK_SRC_TASK_CONNECTED,&self->fsm);
    nn_worker_task_init(&self->task_accept,NN_USOCK_SRC_TASK_ACCEPT,&self->fsm);
    nn_worker_task_init(&self->task_send,NN_USOCK_SRC_TASK_SEND,&self->fsm);
    nn_worker_task_init(&self->task_recv,NN_USOCK_SRC_TASK_RECV,&self->fsm);
    nn_worker_task_init(&self->task_stop,NN_USOCK_SRC_TASK_STOP,&self->fsm);
    // Intialise events raised by usock
    nn_fsm_event_init(&self->event_established);
    nn_fsm_event_init(&self->event_sent);
    nn_fsm_event_init(&self->event_received);
    nn_fsm_event_init(&self->event_error);
    // accepting is not going on at the moment
    self->asock = NULL;
}

void nn_usock_term(struct nn_usock *self)
{
    //printf("nn_usock_term usock.%d\n",self->s);
    nn_assert_state(self,NN_USOCK_STATE_IDLE);
    if ( self->in.batch )
        nn_free(self->in.batch);
    nn_fsm_event_term(&self->event_error);
    nn_fsm_event_term(&self->event_received);
    nn_fsm_event_term(&self->event_sent);
    nn_fsm_event_term(&self->event_established);
    nn_worker_cancel(self->worker,&self->task_recv);
    nn_worker_task_term(&self->task_stop);
    nn_worker_task_term(&self->task_recv);
    nn_worker_task_term(&self->task_send);
    nn_worker_task_term(&self->task_accept);
    nn_worker_task_term(&self->task_connected);
    nn_worker_task_term(&self->task_connecting);
    nn_worker_fd_term(&self->wfd);
    nn_fsm_term(&self->fsm);
}

int32_t nn_usock_isidle(struct nn_usock *self) { return nn_fsm_isidle(&self->fsm); }

int32_t nn_usock_start(struct nn_usock *self,int32_t domain,int32_t type,int32_t protocol)
{
    int32_t s;
    //  If the operating system allows to directly open the socket with CLOEXEC flag, do so. That way there are no race conditions
#ifdef SOCK_CLOEXEC
    type |= SOCK_CLOEXEC;
#endif
    s = socket(domain,type,protocol); // Open the underlying socket
    //printf("start usock.%d\n",s);
    if (nn_slow (s < 0))
       return -errno;
    nn_usock_init_from_fd(self,s);
    nn_fsm_start(&self->fsm); // Start the state machine
    return 0;
}

void nn_usock_start_fd(struct nn_usock *self,int32_t fd)
{
    nn_usock_init_from_fd(self,fd);
    nn_fsm_start(&self->fsm);
    nn_fsm_action(&self->fsm,NN_USOCK_ACTION_STARTED);
}

void nn_errno_assert(int32_t rc)
{
#if defined __APPLE__
    errno_assert (rc != -1 || errno == EINVAL);
#else
    errno_assert (rc != -1);
#endif
}

static void nn_usock_init_from_fd (struct nn_usock *self, int s)
{
    int32_t rc,opt;
    nn_assert (self->state == NN_USOCK_STATE_IDLE || NN_USOCK_STATE_BEING_ACCEPTED);
    nn_assert (self->s == -1);
    self->s = s; // Store the file descriptor
    // Setting FD_CLOEXEC option immediately after socket creation is the second best option after using SOCK_CLOEXEC. There is a race condition here (if process is forked between socket creation and setting the option) but the problem is pretty unlikely to happen
#if defined FD_CLOEXEC
    rc = fcntl (self->s, F_SETFD, FD_CLOEXEC);
    nn_errno_assert(rc);
#endif
    // If applicable, prevent SIGPIPE signal when writing to connection already closed by the peer
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt(self->s,SOL_SOCKET,SO_NOSIGPIPE,&opt,sizeof(opt));
    nn_errno_assert(rc);
#endif
    // Switch socket to the non-blocking mode. All underlying sockets are used in the nonblock mode
    opt = fcntl(self->s,F_GETFL,0);
    if ( opt == -1 )
        opt = 0;
    if ( !(opt & O_NONBLOCK) )
    {
        rc = fcntl(self->s,F_SETFL,opt | O_NONBLOCK);
        nn_errno_assert(rc);
    }
}

void nn_usock_stop(struct nn_usock *self) { nn_fsm_stop(&self->fsm); }

void nn_usock_async_stop(struct nn_usock *self)
{
    nn_worker_execute(self->worker,&self->task_stop);
    nn_fsm_raise(&self->fsm,&self->event_error,NN_USOCK_SHUTDOWN);
}

void nn_usock_swap_owner(struct nn_usock *self,struct nn_fsm_owner *owner)
{
    nn_fsm_swap_owner(&self->fsm,owner);
}

int32_t nn_usock_setsockopt (struct nn_usock *self,int32_t level,int32_t optname,const void *optval,size_t optlen)
{
    int32_t rc;
    // The socket can be modified only before it's active
    nn_assert(self->state == NN_USOCK_STATE_STARTING || self->state == NN_USOCK_STATE_ACCEPTED);
    // EINVAL errors are ignored on OSX platform. The reason for that is buggy OSX behaviour where setsockopt returns EINVAL if the peer have already disconnected. Thus, nn_usock_setsockopt() can succeed on OSX even though the option value was invalid, but the peer have already closed the connection. This behaviour should be relatively harmless.
    rc = setsockopt(self->s,level,optname,optval,(socklen_t)optlen);
#if defined __APPLE__
    if ( nn_slow(rc != 0 && errno != EINVAL) )
        return -errno;
#else
    if ( nn_slow(rc != 0) )
        return -errno;
#endif
    return 0;
}

int32_t nn_usock_bind(struct nn_usock *self,const struct sockaddr *addr,size_t addrlen)
{
    int32_t rc,opt;
    nn_assert_state(self,NN_USOCK_STATE_STARTING); // The socket can be bound only before connected
    opt = 1;
    rc = setsockopt(self->s,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)); // Allow re-using the address
    errno_assert(rc == 0);
    rc = bind(self->s, addr,(socklen_t)addrlen);
    if ( nn_slow(rc != 0) )
        return -errno;
    return 0;
}

int32_t nn_usock_listen(struct nn_usock *self,int32_t backlog)
{
    int32_t rc;
    nn_assert_state(self,NN_USOCK_STATE_STARTING); // You can start listening only before the socket is connected
    rc = listen(self->s, backlog); // Start listening for incoming connections
    if ( nn_slow(rc != 0))
        return -errno;
    nn_fsm_action(&self->fsm,NN_USOCK_ACTION_LISTEN); //  Notify the state machine
    return 0;
}

void nn_usock_accept(struct nn_usock *self,struct nn_usock *listener)
{
    int32_t s;
    if ( nn_fsm_isidle(&self->fsm) ) //  Start the actual accepting
    {
        nn_fsm_start(&self->fsm);
        nn_fsm_action(&self->fsm,NN_USOCK_ACTION_BEING_ACCEPTED);
    }
    nn_fsm_action(&listener->fsm,NN_USOCK_ACTION_ACCEPT);
    //  Try to accept new connection in synchronous manner
#if NN_HAVE_ACCEPT4
    s = accept4(listener->s,NULL,NULL,SOCK_CLOEXEC);
#else
    s = accept(listener->s,NULL,NULL);
#endif
    if ( nn_fast(s >= 0) ) // Immediate success
    {
        // Disassociate the listener socket from the accepted socket. Is useful if we restart accepting on ACCEPT_ERROR
        listener->asock = NULL;
        self->asock = NULL;
        nn_usock_init_from_fd(self,s);
        nn_fsm_action(&listener->fsm,NN_USOCK_ACTION_DONE);
        nn_fsm_action(&self->fsm,NN_USOCK_ACTION_DONE);
        return;
    }
    // Detect a failure. Note that in ECONNABORTED case we simply ignore the error and wait for next connection in asynchronous manner
    errno_assert (errno == EAGAIN || errno == EWOULDBLOCK ||
        errno == ECONNABORTED || errno == ENFILE || errno == EMFILE ||
        errno == ENOBUFS || errno == ENOMEM);
    // Pair the two sockets. They are already paired in case previous attempt failed on ACCEPT_ERROR
    nn_assert(!self->asock || self->asock == listener);
    self->asock = listener;
    nn_assert(!listener->asock || listener->asock == self);
    listener->asock = self;
    // Some errors are just ok to ignore for now.  We also stop repeating any errors until next IN_FD event so that we are not in a tight loop and allow processing other events in the meantime
    if ( nn_slow(errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNABORTED && errno != listener->errnum))
    {
        listener->errnum = errno;
        listener->state = NN_USOCK_STATE_ACCEPTING_ERROR;
        nn_fsm_raise(&listener->fsm,&listener->event_error,NN_USOCK_ACCEPT_ERROR);
        return;
    }
    // Ask the worker thread to wait for the new connection
    nn_worker_execute(listener->worker,&listener->task_accept);
}

void nn_usock_activate(struct nn_usock *self)
{
    //printf("uactivate.%d\n",self->s);
    nn_fsm_action(&self->fsm,NN_USOCK_ACTION_ACTIVATE);
}

void nn_usock_connect(struct nn_usock *self,const struct sockaddr *addr,size_t addrlen)
{
    int32_t rc;
    //printf("Uconnect to usock.%d\n",self->s);
    //  Notify the state machine that we've started connecting
    nn_fsm_action (&self->fsm,NN_USOCK_ACTION_CONNECT);
    rc = connect (self->s,addr,(socklen_t)addrlen); // Do the connect itself
    if ( nn_fast(rc == 0) ) // Immediate success
    {
        nn_fsm_action (&self->fsm,NN_USOCK_ACTION_DONE);
        return;
    }
    if ( nn_slow(errno != EINPROGRESS) ) // Immediate error
    {
        self->errnum = errno;
        nn_fsm_action(&self->fsm,NN_USOCK_ACTION_ERROR);
        return;
    }
    nn_worker_execute (self->worker, &self->task_connecting); // Start asynchronous connect
}

void nn_usock_send(struct nn_usock *self,const struct nn_iovec *iov,int32_t iovcnt)
{
    int32_t rc,i,out;
    // Make sure that the socket is actually alive
    nn_assert_state (self, NN_USOCK_STATE_ACTIVE);
    // Copy the iovecs to the socket
    nn_assert (iovcnt <= NN_USOCK_MAX_IOVCNT);
    self->out.hdr.msg_iov = self->out.iov;
    out = 0;
    //printf("%d iov: ",iovcnt);
    for (i=0; i<iovcnt; i++)
    {
        if (iov [i].iov_len == 0)
            continue;
        self->out.iov[out].iov_base = iov[i].iov_base;
        self->out.iov[out].iov_len = iov[i].iov_len;
        //printf("%d, ",(int32_t)iov[i].iov_len);
        out++;
    }
    //printf("nn_usock_send usock.%d\n",self->s);
    self->out.hdr.msg_iovlen = out;
    if ( (rc= nn_usock_send_raw(self,&self->out.hdr)) == 0 ) // Try to send the data immediately
    {
        nn_fsm_raise(&self->fsm,&self->event_sent,NN_USOCK_SENT);
        return;
    }
    if ( nn_slow(rc != -EAGAIN) ) // Errors
    {
        errnum_assert(rc == -ECONNRESET, -rc);
        nn_fsm_action(&self->fsm,NN_USOCK_ACTION_ERROR);
        return;
    }
    nn_worker_execute(self->worker,&self->task_send); // worker thread to send the remaining data
}

void nn_usock_recv(struct nn_usock *self,void *buf,size_t len,int32_t *fd)
{
    int32_t rc; size_t nbytes;
    nn_assert_state(self,NN_USOCK_STATE_ACTIVE); // Make sure that the socket is actually alive
    nbytes = len;
    self->in.pfd = fd;
    rc = nn_usock_recv_raw(self,buf,&nbytes); // Try to receive the data immediately
    if ( nn_slow(rc < 0) )
    {
        errnum_assert(rc == -ECONNRESET,-rc);
        nn_fsm_action(&self->fsm,NN_USOCK_ACTION_ERROR);
        return;
    }
    //printf("usock.%d nn_usock_recv.[%d %d %d %d] rc.%d nbytes.%d\n",self->s,((uint8_t *)buf)[0],((uint8_t *)buf)[1],((uint8_t *)buf)[2],((uint8_t *)buf)[3],rc,(int32_t)nbytes);
    if ( nn_fast(nbytes == len) ) // Success
    {
        nn_fsm_raise(&self->fsm,&self->event_received,NN_USOCK_RECEIVED);
        return;
    }
    // There are still data to receive in the background
    self->in.buf = ((uint8_t *)buf) + nbytes;
    self->in.len = len - nbytes;
    nn_worker_execute(self->worker,&self->task_recv); // worker thread to receive the remaining data
}

static int32_t nn_internal_tasks(struct nn_usock *usock,int32_t src,int32_t type)
{
/******************************************************************************/
/*  Internal tasks sent from the user thread to the worker thread.            */
/******************************************************************************/
    switch ( src )
    {
    case NN_USOCK_SRC_TASK_SEND:
        nn_assert (type == NN_WORKER_TASK_EXECUTE);
        nn_worker_set_out (usock->worker, &usock->wfd);
        return 1;
    case NN_USOCK_SRC_TASK_RECV:
        //printf("nn_internal_tasks: NN_USOCK_SRC_TASK_RECV type.%d\n",type);
        nn_assert (type == NN_WORKER_TASK_EXECUTE);
        nn_worker_set_in(usock->worker,&usock->wfd);
        return 1;
    case NN_USOCK_SRC_TASK_CONNECTED:
        nn_assert (type == NN_WORKER_TASK_EXECUTE);
        nn_worker_add_fd (usock->worker, usock->s, &usock->wfd);
        return 1;
    case NN_USOCK_SRC_TASK_CONNECTING:
        nn_assert (type == NN_WORKER_TASK_EXECUTE);
        nn_worker_add_fd (usock->worker, usock->s, &usock->wfd);
        nn_worker_set_out (usock->worker, &usock->wfd);
        return 1;
    case NN_USOCK_SRC_TASK_ACCEPT:
        //printf("nn_internal_tasks: NN_USOCK_SRC_TASK_ACCEPT type.%d\n",type);
        nn_assert (type == NN_WORKER_TASK_EXECUTE);
        nn_worker_add_fd(usock->worker, usock->s, &usock->wfd);
        nn_worker_set_in(usock->worker,&usock->wfd);
        return 1;
    }
    return 0;
}

static void nn_usock_shutdown (struct nn_fsm *self, int src, int type,NN_UNUSED void *srcptr)
{
    struct nn_usock *usock;
    usock = nn_cont(self,struct nn_usock,fsm);
    //printf("nn_usock_shutdown call internal src.%d type.%d\n",src,type);
    if ( nn_internal_tasks(usock,src,type) )
        return;
    if ( nn_slow(src == NN_FSM_ACTION && type == NN_FSM_STOP) )
    {
        // Socket in ACCEPTING or CANCELLING state cannot be closed. Stop the socket being accepted first
        nn_assert(usock->state != NN_USOCK_STATE_ACCEPTING && usock->state != NN_USOCK_STATE_CANCELLING);
        usock->errnum = 0;
        // Synchronous stop
        if ( usock->state == NN_USOCK_STATE_IDLE )
            goto finish3;
        if ( usock->state == NN_USOCK_STATE_DONE )
            goto finish2;
        if ( usock->state == NN_USOCK_STATE_STARTING ||
              usock->state == NN_USOCK_STATE_ACCEPTED ||
              usock->state == NN_USOCK_STATE_ACCEPTING_ERROR ||
              usock->state == NN_USOCK_STATE_LISTENING)
            goto finish1;
        // When socket that's being accepted is asked to stop, we have to ask the listener socket to stop accepting first
        if ( usock->state == NN_USOCK_STATE_BEING_ACCEPTED )
        {
            nn_fsm_action(&usock->asock->fsm,NN_USOCK_ACTION_CANCEL);
            usock->state = NN_USOCK_STATE_STOPPING_ACCEPT;
            return;
        }
        if ( usock->state != NN_USOCK_STATE_REMOVING_FD ) // Asynchronous stop
            nn_usock_async_stop(usock);
        usock->state = NN_USOCK_STATE_STOPPING;
        return;
    }
    if ( nn_slow(usock->state == NN_USOCK_STATE_STOPPING_ACCEPT) )
    {
        nn_assert (src == NN_FSM_ACTION && type == NN_USOCK_ACTION_DONE);
        goto finish2;
    }
    if ( nn_slow(usock->state == NN_USOCK_STATE_STOPPING) )
    {
        if ( src != NN_USOCK_SRC_TASK_STOP )
            return;
        nn_assert(type == NN_WORKER_TASK_EXECUTE);
        nn_worker_rm_fd(usock->worker,&usock->wfd);
finish1:
        nn_closefd(usock->s);
        usock->s = -1;
finish2:
        usock->state = NN_USOCK_STATE_IDLE;
        nn_fsm_stopped(&usock->fsm,NN_USOCK_STOPPED);
finish3:
        return;
    }
    nn_fsm_bad_state(usock->state, src, type);
}

static void nn_usock_handler(struct nn_fsm *self,int32_t src,int32_t type,NN_UNUSED void *srcptr)
{
    int32_t rc,s,sockerr; struct nn_usock *usock; size_t sz;
    //printf("nn_usock_handler call internal src.%d type.%d\n",src,type);
    usock = nn_cont(self,struct nn_usock,fsm);
    if ( nn_internal_tasks(usock,src,type) )
        return;
    switch ( usock->state )
    {
/******************************************************************************/
/*  IDLE state.                                                               */
/*  nn_usock object is initialised, but underlying OS socket is not yet       */
/*  created.                                                                  */
/******************************************************************************/
    case NN_USOCK_STATE_IDLE:
        switch ( src )
        {
        case NN_FSM_ACTION:
            switch ( type )
            {
            case NN_FSM_START:
                usock->state = NN_USOCK_STATE_STARTING;
                return;
            default:
                nn_fsm_bad_action(usock->state, src, type);
            }
        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  STARTING state.                                                           */
/*  Underlying OS socket is created, but it's not yet passed to the worker    */
/*  thread. In this state we can set socket options, local and remote         */
/*  address etc.                                                              */
/******************************************************************************/
    case NN_USOCK_STATE_STARTING:

        /*  Events from the owner of the usock. */
        switch (src) {
        case NN_FSM_ACTION:
            switch (type) {
            case NN_USOCK_ACTION_LISTEN:
                usock->state = NN_USOCK_STATE_LISTENING;
                return;
            case NN_USOCK_ACTION_CONNECT:
                usock->state = NN_USOCK_STATE_CONNECTING;
                return;
            case NN_USOCK_ACTION_BEING_ACCEPTED:
                usock->state = NN_USOCK_STATE_BEING_ACCEPTED;
                return;
            case NN_USOCK_ACTION_STARTED:
                nn_worker_add_fd (usock->worker, usock->s, &usock->wfd);
                usock->state = NN_USOCK_STATE_ACTIVE;
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  BEING_ACCEPTED state.                                                     */
/*  accept() was called on the usock. Now the socket is waiting for a new     */
/*  connection to arrive.                                                     */
/******************************************************************************/
    case NN_USOCK_STATE_BEING_ACCEPTED:
        switch (src) {
        case NN_FSM_ACTION:
            switch (type) {
            case NN_USOCK_ACTION_DONE:
                usock->state = NN_USOCK_STATE_ACCEPTED;
                nn_fsm_raise (&usock->fsm, &usock->event_established,
                    NN_USOCK_ACCEPTED);
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  ACCEPTED state.                                                           */
/*  Connection was accepted, now it can be tuned. Afterwards, it'll move to   */
/*  the active state.                                                         */
/******************************************************************************/
    case NN_USOCK_STATE_ACCEPTED:
        switch (src) {
        case NN_FSM_ACTION:
            switch (type) {
            case NN_USOCK_ACTION_ACTIVATE:
                nn_worker_add_fd (usock->worker, usock->s, &usock->wfd);
                usock->state = NN_USOCK_STATE_ACTIVE;
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  CONNECTING state.                                                         */
/*  Asynchronous connecting is going on.                                      */
/******************************************************************************/
    case NN_USOCK_STATE_CONNECTING:
        switch (src) {
        case NN_FSM_ACTION:
            switch (type) {
            case NN_USOCK_ACTION_DONE:
                usock->state = NN_USOCK_STATE_ACTIVE;
                nn_worker_execute (usock->worker, &usock->task_connected);
                nn_fsm_raise (&usock->fsm, &usock->event_established,
                    NN_USOCK_CONNECTED);
                return;
            case NN_USOCK_ACTION_ERROR:
                nn_closefd (usock->s);
                usock->s = -1;
                usock->state = NN_USOCK_STATE_DONE;
                nn_fsm_raise (&usock->fsm, &usock->event_error,
                    NN_USOCK_ERROR);
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        case NN_USOCK_SRC_FD:
            switch (type) {
            case NN_WORKER_FD_OUT:
                nn_worker_reset_out (usock->worker, &usock->wfd);
                usock->state = NN_USOCK_STATE_ACTIVE;
                sockerr = nn_usock_geterr(usock);
                if (sockerr == 0) {
                    nn_fsm_raise (&usock->fsm, &usock->event_established,
                        NN_USOCK_CONNECTED);
                } else {
                    usock->errnum = sockerr;
                    nn_worker_rm_fd (usock->worker, &usock->wfd);
                    rc = close (usock->s);
                    errno_assert (rc == 0);
                    usock->s = -1;
                    usock->state = NN_USOCK_STATE_DONE;
                    nn_fsm_raise (&usock->fsm,
                        &usock->event_error, NN_USOCK_ERROR);
                }
                return;
            case NN_WORKER_FD_ERR:
                nn_worker_rm_fd (usock->worker, &usock->wfd);
                nn_closefd (usock->s);
                usock->s = -1;
                usock->state = NN_USOCK_STATE_DONE;
                nn_fsm_raise (&usock->fsm, &usock->event_error, NN_USOCK_ERROR);
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  ACTIVE state.                                                             */
/*  Socket is connected. It can be used for sending and receiving data.       */
/******************************************************************************/
    case NN_USOCK_STATE_ACTIVE:
        switch (src) {
        case NN_USOCK_SRC_FD:
            switch (type) {
            case NN_WORKER_FD_IN:
                sz = usock->in.len;
                rc = nn_usock_recv_raw(usock,usock->in.buf,&sz);
                //printf("NN_USOCK_STATE_ACTIVE FD_IN[%d] (%d %d %d %d) sz.%d\n",rc,usock->in.buf[0],usock->in.buf[1],usock->in.buf[2],usock->in.buf[3],(int32_t)sz);
                if ( nn_fast(rc == 0) )
                {
                    usock->in.len -= sz;
                    usock->in.buf += sz;
                    if ( !usock->in.len )
                    {
                        nn_worker_reset_in(usock->worker,&usock->wfd);
                        nn_fsm_raise(&usock->fsm,&usock->event_received,NN_USOCK_RECEIVED);
                    }
                    return;
                }
                errnum_assert (rc == -ECONNRESET, -rc);
                goto error;
            case NN_WORKER_FD_OUT:
                rc = nn_usock_send_raw (usock, &usock->out.hdr);
                if (nn_fast (rc == 0)) {
                    nn_worker_reset_out (usock->worker, &usock->wfd);
                    nn_fsm_raise (&usock->fsm, &usock->event_sent,
                        NN_USOCK_SENT);
                    return;
                }
                if (nn_fast (rc == -EAGAIN))
                    return;
                errnum_assert (rc == -ECONNRESET, -rc);
                goto error;
            case NN_WORKER_FD_ERR:
error:
                nn_worker_rm_fd (usock->worker, &usock->wfd);
                nn_closefd (usock->s);
                usock->s = -1;
                usock->state = NN_USOCK_STATE_DONE;
                nn_fsm_raise (&usock->fsm, &usock->event_error, NN_USOCK_ERROR);
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        case NN_FSM_ACTION:
            switch (type) {
            case NN_USOCK_ACTION_ERROR:
                usock->state = NN_USOCK_STATE_REMOVING_FD;
                nn_usock_async_stop (usock);
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        default:
            nn_fsm_bad_source(usock->state, src, type);
        }

/******************************************************************************/
/*  REMOVING_FD state.                                                        */
/******************************************************************************/
    case NN_USOCK_STATE_REMOVING_FD:
        switch (src) {
        case NN_USOCK_SRC_TASK_STOP:
            switch (type) {
            case NN_WORKER_TASK_EXECUTE:
                nn_worker_rm_fd (usock->worker, &usock->wfd);
                nn_closefd (usock->s);
                usock->s = -1;
                usock->state = NN_USOCK_STATE_DONE;
                nn_fsm_raise (&usock->fsm, &usock->event_error, NN_USOCK_ERROR);
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }

        /*  Events from the file descriptor are ignored while it is being
            removed. */
        case NN_USOCK_SRC_FD:
            return;

        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  DONE state.                                                               */
/*  Socket is closed. The only thing that can be done in this state is        */
/*  stopping the usock.                                                       */
/******************************************************************************/
    case NN_USOCK_STATE_DONE:
        nn_fsm_bad_source (usock->state, src, type);

/******************************************************************************/
/*  LISTENING state.                                                          */
/*  Socket is listening for new incoming connections, however, user is not    */
/*  accepting a new connection.                                               */
/******************************************************************************/
    case NN_USOCK_STATE_LISTENING:
        switch (src) {
        case NN_FSM_ACTION:
            switch (type) {
            case NN_USOCK_ACTION_ACCEPT:
                usock->state = NN_USOCK_STATE_ACCEPTING;
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  ACCEPTING state.                                                          */
/*  User is waiting asynchronouslyfor a new inbound connection                */
/*  to be accepted.                                                           */
/******************************************************************************/
    case NN_USOCK_STATE_ACCEPTING:
        switch (src) {
        case NN_FSM_ACTION:
            switch (type) {
            case NN_USOCK_ACTION_DONE:
                usock->state = NN_USOCK_STATE_LISTENING;
                return;
            case NN_USOCK_ACTION_CANCEL:
                usock->state = NN_USOCK_STATE_CANCELLING;
                nn_worker_execute (usock->worker, &usock->task_stop);
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        case NN_USOCK_SRC_FD:
            switch (type) {
            case NN_WORKER_FD_IN:

                /*  New connection arrived in asynchronous manner. */
#if NN_HAVE_ACCEPT4
                s = accept4 (usock->s, NULL, NULL, SOCK_CLOEXEC);
#else
                s = accept (usock->s, NULL, NULL);
#endif

                /*  ECONNABORTED is an valid error. New connection was closed
                    by the peer before we were able to accept it. If it happens
                    do nothing and wait for next incoming connection. */
                if (nn_slow (s < 0 && errno == ECONNABORTED))
                    return;

                /*  Resource allocation errors. It's not clear from POSIX
                    specification whether the new connection is closed in this
                    case or whether it remains in the backlog. In the latter
                    case it would be wise to wait here for a while to prevent
                    busy looping. */
                if (nn_slow (s < 0 && (errno == ENFILE || errno == EMFILE ||
                      errno == ENOBUFS || errno == ENOMEM))) {
                    usock->errnum = errno;
                    usock->state = NN_USOCK_STATE_ACCEPTING_ERROR;

                    /*  Wait till the user starts accepting once again. */
                    nn_worker_rm_fd (usock->worker, &usock->wfd);

                    nn_fsm_raise (&usock->fsm,
                        &usock->event_error, NN_USOCK_ACCEPT_ERROR);
                    return;
                }

                /* Any other error is unexpected. */
                errno_assert (s >= 0);

                /*  Initialise the new usock object. */
                nn_usock_init_from_fd (usock->asock, s);
                usock->asock->state = NN_USOCK_STATE_ACCEPTED;

                /*  Notify the user that connection was accepted. */
                nn_fsm_raise (&usock->asock->fsm,
                    &usock->asock->event_established, NN_USOCK_ACCEPTED);

                /*  Disassociate the listener socket from the accepted
                    socket. */
                usock->asock->asock = NULL;
                usock->asock = NULL;

                /*  Wait till the user starts accepting once again. */
                nn_worker_rm_fd (usock->worker, &usock->wfd);
                usock->state = NN_USOCK_STATE_LISTENING;

                return;

            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  ACCEPTING_ERROR state.                                                    */
/*  Waiting the socket to accept the error and restart                        */
/******************************************************************************/
    case NN_USOCK_STATE_ACCEPTING_ERROR:
        switch (src) {
        case NN_FSM_ACTION:
            switch (type) {
            case NN_USOCK_ACTION_ACCEPT:
                usock->state = NN_USOCK_STATE_ACCEPTING;
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  CANCELLING state.                                                         */
/******************************************************************************/
    case NN_USOCK_STATE_CANCELLING:
        switch (src) {
        case NN_USOCK_SRC_TASK_STOP:
            switch (type) {
            case NN_WORKER_TASK_EXECUTE:
                nn_worker_rm_fd (usock->worker, &usock->wfd);
                usock->state = NN_USOCK_STATE_LISTENING;

                /*  Notify the accepted socket that it was stopped. */
                nn_fsm_action (&usock->asock->fsm, NN_USOCK_ACTION_DONE);

                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        case NN_USOCK_SRC_FD:
            switch (type) {
            case NN_WORKER_FD_IN:
                return;
            default:
                nn_fsm_bad_action (usock->state, src, type);
            }
        default:
            nn_fsm_bad_source (usock->state, src, type);
        }

/******************************************************************************/
/*  Invalid state                                                             */
/******************************************************************************/
    default:
        nn_fsm_bad_state (usock->state, src, type);
    }
}

int32_t nn_getiovec_size(uint8_t *buf,int32_t maxlen,struct msghdr *hdr)
{
    int32_t i,size = 0; struct iovec *iov;
    for (i=0; i<hdr->msg_iovlen; i++)
    {
        iov = &hdr->msg_iov[i];
        if ( nn_slow(iov->iov_len == NN_MSG) )
        {
            errno = EINVAL;
            printf("ERROR: iov->iov_len == NN_MSG\n");
            return(-1);
        }
        if ( nn_slow(!iov->iov_base && iov->iov_len) )
        {
            errno = EFAULT;
            printf("ERROR: !iov->iov_base && iov->iov_len\n");
            return(-1);
        }
        if ( maxlen > 0 && nn_slow(size + iov->iov_len > maxlen) )
        {
            errno = EINVAL;
            printf("ERROR: sz.%d + iov->iov_len.%d < maxlen.%d\n",(int32_t)size,(int32_t)iov->iov_len,maxlen);
            return(-1);
        }
        if ( iov->iov_len > 0 )
        {
            if ( buf != 0 )
                memcpy(&buf[size],iov->iov_base,iov->iov_len);
            size += (int32_t)iov->iov_len;
        }
    }
    return(size);
}

ssize_t mysendmsg(int32_t usock,struct msghdr *hdr,int32_t flags)
{
    ssize_t nbytes = 0; int32_t veclen,offset,clen,err = 0; uint8_t *buf,_buf[8192];
    if ( (veclen= nn_getiovec_size(0,0,hdr)) > 0 )
    {
        clen = hdr->msg_controllen;
        if ( hdr->msg_control == 0 )
            clen = 0;
        if ( veclen > (sizeof(_buf) - clen - 6) )
            buf = malloc(veclen + clen + 6);
        else buf = _buf;
        offset = 0;
        buf[offset++] = (veclen & 0xff);
        buf[offset++] = ((veclen>>8) & 0xff);
        buf[offset++] = ((veclen>>15) & 0xff);
        buf[offset++] = (clen & 0xff);
        buf[offset++] = ((clen>>8) & 0xff);
        if ( clen > 0 )
            memcpy(&buf[offset],hdr->msg_control,clen), offset += clen;
        if ( nn_getiovec_size(&buf[offset],veclen,hdr) == veclen )
        {
            nbytes = send(usock,buf,offset + veclen,0);
            printf(">>>>>>>>> send.[%d %d %d %d] (n.%d v.%d c.%d)-> usock.%d nbytes.%d\n",buf[offset],buf[offset+1],buf[offset+2],buf[offset+3],(int32_t)offset+veclen,veclen,clen,usock,(int32_t)nbytes);
            if ( nbytes == offset + veclen )
                nbytes = veclen;
            else
            {
                printf("nbytes.%d != offset.%d veclen.%d\n",(int32_t)nbytes,(int32_t)offset,veclen);
            }
        }
        else err = -errno;
        if ( buf != _buf )
            free(buf);
        if ( err != 0 )
        {
            printf("nn_usock_send_raw errno.%d err.%d\n",errno,err);
            return(-errno);
        }
    }
    else
    {
        printf("nn_usock_send_raw errno.%d invalid iovec size\n",errno);
        return(-errno);
    }
    return(nbytes);
}

ssize_t myrecvmsg(int32_t usock,struct msghdr *hdr,int32_t flags)
{
    int32_t offset,veclen,clen,cbytes,n; ssize_t nbytes; struct iovec *iov; uint8_t lens[5];
    iov = hdr->msg_iov;
    if ( (n= (int32_t)recv(usock,lens,sizeof(lens),0)) != sizeof(lens) )
    {
        printf("error getting veclen/clen n.%d vs %d from usock.%d\n",n,(int32_t)sizeof(lens),usock);
        return(0);
    } else printf("GOT %d bytes from usock.%d\n",n,usock);
    offset = 0;
    veclen = ((uint8_t *)iov->iov_base)[offset++];
    veclen |= ((int32_t)((uint8_t *)iov->iov_base)[offset++] << 8);
    veclen |= ((int32_t)((uint8_t *)iov->iov_base)[offset++] << 16);
    clen = ((uint8_t *)iov->iov_base)[offset++];
    clen |= ((int32_t)((uint8_t *)iov->iov_base)[offset++] << 8);
    printf("veclen.%d clen.%d waiting in usock.%d\n",veclen,clen,usock);
    if ( clen > 0 )
    {
        if ( (cbytes= (int32_t)recv(usock,hdr->msg_control,clen,0)) != clen )
        {
            printf("myrecvmsg: unexpected cbytes.%d vs clen.%d\n",cbytes,clen);
            return(0);
        }
        hdr->msg_controllen = clen;
    }
    if ( (nbytes= (int32_t)recv(usock,iov->iov_base,veclen,0)) != veclen )
    {
        printf("myrecvmsg: unexpected nbytes.%d vs clen.%d\n",(int32_t)nbytes,veclen);
        return(0);
    }
    printf("GOT cbytes.%d nbytes.%d\n",(int32_t)cbytes,(int32_t)nbytes);
    if ( nbytes > 0 )
    {
        printf("got nbytes.%d from usock.%d [%d %d %d %d]\n",(int32_t)nbytes,usock,((uint8_t *)iov->iov_base)[0],((uint8_t *)iov->iov_base)[1],((uint8_t *)iov->iov_base)[2],((uint8_t *)iov->iov_base)[3]);
    }
    return(nbytes);
}

static int32_t nn_usock_send_raw(struct nn_usock *self,struct msghdr *hdr)
{
    ssize_t offset = 0,nbytes = 0;
#if defined __PNACL 
    //|| defined __APPLE__
    int32_t veclen,clen,err = 0; uint8_t *buf,_buf[8192];
    if ( (veclen= nn_getiovec_size(0,0,hdr)) > 0 )
    {
        clen = hdr->msg_controllen;
        if ( hdr->msg_control == 0 )
            clen = 0;
        if ( veclen > (sizeof(_buf) - clen - 5) )
            buf = malloc(veclen + clen + 5);
        else buf = _buf;
        buf[offset++] = (veclen & 0xff);
        buf[offset++] = ((veclen>>8) & 0xff);
        buf[offset++] = ((veclen>>15) & 0xff);
        buf[offset++] = (clen & 0xff);
        buf[offset++] = ((clen>>8) & 0xff);
        if ( clen > 0 && hdr->msg_control != 0 )
            memcpy(&buf[offset],hdr->msg_control,clen), offset += clen;
        if ( nn_getiovec_size(&buf[offset],veclen,hdr) == veclen )
        {
            nbytes = send(self->s,buf,offset + veclen,0);
            printf(">>>>>>>>> send.[%d %d %d %d] (n.%d v.%d c.%d)-> usock.%d nbytes.%d\n",buf[offset],buf[offset+1],buf[offset+2],buf[offset+3],(int32_t)offset+veclen,veclen,clen,self->s,(int32_t)nbytes);
            if ( nbytes == offset + veclen )
                nbytes = veclen;
            else
            {
                printf("nbytes.%d != offset.%d veclen.%d\n",(int32_t)nbytes,(int32_t)offset,veclen);
            }
        }
        else err = -errno;
        if ( buf != _buf )
            free(buf);
        if ( err != 0 )
        {
            printf("nn_usock_send_raw errno.%d err.%d\n",errno,err);
            return(-errno);
        }
    }
    else
    {
        printf("nn_usock_send_raw errno.%d invalid iovec size\n",errno);
        return(-errno);
    }
#else
#if defined MSG_NOSIGNAL
    nbytes = sendmsg(self->s,hdr,MSG_NOSIGNAL);
#else
    nbytes = sendmsg(self->s,hdr,0);
#endif
#endif
    //printf("nn_usock_send_raw nbytes.%d errno.%d for sock.%d\n",(int32_t)nbytes,errno,self->s);
    if ( nn_slow(nbytes < 0) ) // Handle errors
    {
        if ( nn_fast(errno == EAGAIN || errno == EWOULDBLOCK) )
            nbytes = 0;
/*#ifdef __PNACL
        else if ( errno < 0 )
        {
            PostMessage("nn_usock_send_raw err.%d\n",(int32_t)nbytes);
            printf("nn_usock_send_raw err.%d\n",(int32_t)nbytes);
            return -ECONNRESET;
        }
#endif*/
        else
        {
            //  If the connection fails, return ECONNRESET
            errno_assert(errno == ECONNRESET || errno == ETIMEDOUT || errno == EPIPE ||  errno == ECONNREFUSED || errno == ENOTCONN);
            return -ECONNRESET;
        }
    }
    while ( nbytes != 0 ) // Some bytes sent. Adjust the iovecs. jl777: what to do with ctrl data?
    {
        if ( nbytes >= (ssize_t)hdr->msg_iov->iov_len )
        {
            --hdr->msg_iovlen;
            if ( !hdr->msg_iovlen )
            {
                nn_assert(nbytes == (ssize_t)hdr->msg_iov->iov_len);
                return(0);
            }
            nbytes -= hdr->msg_iov->iov_len;
            ++hdr->msg_iov;
        }
        else
        {
            *((uint8_t **)&(hdr->msg_iov->iov_base)) += nbytes;
            hdr->msg_iov->iov_len -= nbytes;
            return -EAGAIN;
        }
    }
    if ( hdr->msg_iovlen > 0 )
        return -EAGAIN;
    return(0);
}

static int nn_usock_recv_raw(struct nn_usock *self,void *buf,size_t *len)
{
    size_t sz,length; ssize_t nbytes; struct iovec iov; struct msghdr hdr; unsigned char ctrl[256];
#if defined NN_HAVE_MSG_CONTROL
    struct cmsghdr *cmsg;
#endif
    // If batch buffer doesn't exist, allocate it. The point of delayed deallocation to allow non-receiving sockets, such as TCP listening sockets, to do without the batch buffer
    if (nn_slow (!self->in.batch)) {
        self->in.batch = nn_alloc (NN_USOCK_BATCH_SIZE, "AIO batch buffer");
        alloc_assert (self->in.batch);
    }
    // Try to satisfy the recv request by data from the batch buffer
    length = *len;
    sz = self->in.batch_len - self->in.batch_pos;
    if (sz) {
        if (sz > length)
            sz = length;
        memcpy (buf, self->in.batch + self->in.batch_pos, sz);
        self->in.batch_pos += sz;
        buf = ((char*) buf) + sz;
        length -= sz;
        if (!length)
            return 0;
    }
    // If recv request is greater than the batch buffer, get the data directly into the place. Otherwise, read data to the batch buffer
    if (length > NN_USOCK_BATCH_SIZE) {
        iov.iov_base = buf;
        iov.iov_len = length;
    }
    else {
        iov.iov_base = self->in.batch;
        iov.iov_len = NN_USOCK_BATCH_SIZE;
    }
    memset (&hdr, 0, sizeof (hdr));
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
#if defined NN_HAVE_MSG_CONTROL
    hdr.msg_control = ctrl;
    hdr.msg_controllen = sizeof (ctrl);
#else
    *((int*) ctrl) = -1;
    hdr.msg_accrights = ctrl;
    hdr.msg_accrightslen = sizeof (int);
#endif
    nbytes = recvmsg (self->s, &hdr, 0);
    if ( nn_slow(nbytes <= 0) ) // Handle any possible errors
    {
        
        if ( nn_slow(nbytes == 0) )
            return -ECONNRESET;
        if ( nn_fast(errno == EAGAIN || errno == EWOULDBLOCK) ) // Zero bytes received
            nbytes = 0;
        else
        {
            // If the peer closes the connection, return ECONNRESET
            errno_assert (errno == ECONNRESET || errno == ENOTCONN || errno == ECONNREFUSED || errno == ETIMEDOUT || errno == EHOSTUNREACH);
            return -ECONNRESET;
        }
    }
    // Extract the associated file descriptor, if any
    if (nbytes > 0) {
#if defined NN_HAVE_MSG_CONTROL
        cmsg = CMSG_FIRSTHDR (&hdr);
        while (cmsg) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                if (self->in.pfd) {
                    *self->in.pfd = *((int*) CMSG_DATA (cmsg));
                    self->in.pfd = NULL;
                }
                else {
                    nn_closefd (*((int*) CMSG_DATA (cmsg)));
                }
                break;
            }
            cmsg = CMSG_NXTHDR (&hdr, cmsg);
        }
#else
        if (hdr.msg_accrightslen > 0) {
            nn_assert (hdr.msg_accrightslen == sizeof (int));
            if (self->in.pfd) {
                *self->in.pfd = *((int*) hdr.msg_accrights);
                self->in.pfd = NULL;
            }
            else {
                nn_closefd (*((int*) hdr.msg_accrights));
            }
        }
#endif
    }
    // If the data were received directly into the place we can return straight away
    if (length > NN_USOCK_BATCH_SIZE) {
        length -= nbytes;
        *len -= length;
        return 0;
    }
    // New data were read to the batch buffer. Copy the requested amount of it to the user-supplied buffer
    self->in.batch_len = nbytes;
    self->in.batch_pos = 0;
    if (nbytes) {
        sz = nbytes > (ssize_t)length ? length : (size_t)nbytes;
        memcpy (buf, self->in.batch, sz);
        length -= sz;
        self->in.batch_pos += sz;
    }
    *len -= length;
    return 0;
}

int32_t new_usock_recv_raw(struct nn_usock *self,void *buf,size_t *len)
{
    size_t sz,length; int32_t clen,veclen,offset = 0; ssize_t nbytes; struct iovec iov;
    struct msghdr hdr; uint8_t ctrl[256];
    // Try to satisfy the recv request by data from the batch buffer
    length = *len;
    sz = self->in.batch_len - self->in.batch_pos;
    printf("sz.%d = batch.(%d - %d)\n",(int32_t)sz,(int32_t)self->in.batch_len,(int32_t)self->in.batch_pos);
#if defined __PNACL || defined __APPLE__
    nn_assert(sz == 0);
#else
    // If batch buffer doesn't exist, allocate it. The point of delayed deallocation to allow non-receiving sockets, such as TCP listening sockets, to do without the batch buffer
    if ( nn_slow(!self->in.batch) )
    {
        self->in.batch = nn_alloc(NN_USOCK_BATCH_SIZE,"AIO batch buffer");
        alloc_assert(self->in.batch);
    }
    if ( sz )
    {
        if ( sz > length )
            sz = length;
        memcpy(buf,self->in.batch + self->in.batch_pos,sz);
        printf("nn_usock_recv_raw.[%d %d %d %d | %d %d %d %d] pos.%d len.%d sz.%d length.%d <<<<<<<<<\n",((uint8_t *)buf)[0],((uint8_t *)buf)[1],((uint8_t *)buf)[2],((uint8_t *)buf)[3],((uint8_t *)buf)[4],((uint8_t *)buf)[5],((uint8_t *)buf)[6],((uint8_t *)buf)[7],(int32_t)self->in.batch_pos,(int32_t)self->in.batch_len,(int32_t)sz,(int32_t)length);
        self->in.batch_pos += sz;
        buf = ((char *)buf) + sz;
        length -= sz;
        printf("length.%d sz.%d pos.%d\n",(int32_t)length,(int32_t)sz,(int32_t)self->in.batch_pos);
        if ( length == 0 )
            return 0;
    }
    self->in.batch_pos = 0;
    memset(&hdr,0,sizeof(hdr));
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    // If recv request is greater than the batch buffer, get the data directly into the place. Otherwise, read data to the batch buffer
    if ( length > NN_USOCK_BATCH_SIZE )
    {
        iov.iov_base = buf;
        iov.iov_len = length;
        usebuf = 1;
    }
    else
    {
        iov.iov_base = self->in.batch;
        iov.iov_len = NN_USOCK_BATCH_SIZE;
        self->in.batch_pos = 0;
        usebuf = 0;
    }
#if defined NN_HAVE_MSG_CONTROL
    hdr.msg_control = ctrl;
    hdr.msg_controllen = sizeof(ctrl);
#else
    *((int *)ctrl) = -1;
    hdr.msg_accrights = ctrl;
    hdr.msg_accrightslen = sizeof(int32_t);
#endif
    nbytes = (int32_t)recvmsg(self->s,&hdr,0);
    if ( usebuf == 0 )
        self->in.batch_len = nbytes;
    printf("RECVMSG.%d %d bytes errno.%d\n",self->s,(int32_t)nbytes,nbytes < 0 ? errno : 0);
    // Handle any possible errors
    if ( nn_slow(nbytes <= 0) )
    {
        if ( nn_slow(nbytes == 0) )
            return -ECONNRESET;
        if ( nn_fast(errno == EAGAIN || errno == EWOULDBLOCK) ) // Zero bytes received
            nbytes = 0;
        else
        {
            // If the peer closes the connection, return ECONNRESET
            errno_assert(errno == ECONNRESET || errno == ENOTCONN || errno == ECONNREFUSED || errno == ETIMEDOUT || errno == EHOSTUNREACH);
            return -ECONNRESET;
        }
    } else nn_process_cmsg(self,&hdr);
    if ( usebuf != 0 ) // If data were received directly into the place we can return straight away
    {
        length -= nbytes;
        *len -= length;
        return 0;
    }
    // New data were read to the batch buffer. Copy requested amount of it to the user buffer
    if ( nbytes )
    {
        sz = nbytes > (ssize_t)length ? length : (size_t)nbytes;
        memcpy(buf,self->in.batch + self->in.batch_pos,sz);
        printf("%d.[%d %d %d %d] <<<<<<<< sz.%d length.%d ",(int32_t)self->in.batch_pos,((uint8_t *)buf)[0],((uint8_t *)buf)[1],((uint8_t *)buf)[2],((uint8_t *)buf)[3],(int32_t)sz,(int32_t)length);
        length -= sz;
        self->in.batch_pos += sz;
    }
    *len -= length;
    if ( nbytes != 0 || *len != 0 || self->in.batch_pos != 0 || self->in.batch_len != 0 )
        printf("nbytes.%d len.%d | self->in.batch_pos.%d self->in.batch_len.%d\n",(int32_t)nbytes,(int32_t)*len,(int32_t)self->in.batch_pos,(int32_t)self->in.batch_len);
    return 0;
#endif
#if defined __PNACL || defined __APPLE__
    offset = 0;
    iov.iov_base = self->in.batch;
    iov.iov_len = NN_USOCK_BATCH_SIZE; //-2-sizeof(ctrl);
    nbytes = (int32_t)recv(self->s,iov.iov_base,iov.iov_len,0);
    printf("rawnbytes.%d ",(int32_t)nbytes);
    self->in.batch_len = nbytes >= 0 ? nbytes : 0;
    hdr.msg_controllen = 0;
    hdr.msg_control = ctrl;
    if ( nbytes > 0 )
    {
        printf("got nbytes.%d from usock.%d [%d %d %d %d]\n",(int32_t)nbytes,self->s,((uint8_t *)iov.iov_base)[0],((uint8_t *)iov.iov_base)[1],((uint8_t *)iov.iov_base)[2],((uint8_t *)iov.iov_base)[3]);
        offset = 0;
        veclen = ((uint8_t *)iov.iov_base)[offset++];
        veclen |= ((int32_t)((uint8_t *)iov.iov_base)[offset++] << 8);
        veclen |= ((int32_t)((uint8_t *)iov.iov_base)[offset++] << 16);
        clen = ((uint8_t *)iov.iov_base)[offset++];
        clen |= ((int32_t)((uint8_t *)iov.iov_base)[offset++] << 8);
        if ( clen > 0 )
        {
            if ( clen > sizeof(ctrl) )
            {
                printf("too much control data.%d vs %d, truncate\n",clen,(int32_t)sizeof(ctrl));
                memcpy(ctrl,&((uint8_t *)iov.iov_base)[offset],sizeof(ctrl));
                errno = MSG_CTRUNC;
            }
            else memcpy(ctrl,&((uint8_t *)iov.iov_base)[offset],clen);
        }
        hdr.msg_controllen = clen;
        nbytes -= offset;
        printf("offset.%d nbytes.%d clen.%d\n",offset,(int32_t)nbytes,clen);
        self->in.batch_pos = offset;
    } else self->in.batch_pos = 0;
    return((int32_t)nbytes);
#endif
}

static int nn_usock_geterr (struct nn_usock *self)
{
    int32_t rc,opt;
#if defined NN_HAVE_HPUX
    int32_t optsz;
#else
    socklen_t optsz;
#endif
    opt = 0;
    optsz = sizeof(opt);
    rc = getsockopt(self->s,SOL_SOCKET,SO_ERROR,&opt,&optsz);
    //  The following should handle both Solaris and UNIXes derived from BSD
    if ( rc == -1 )
        return errno;
    errno_assert(rc == 0);
    nn_assert(optsz == sizeof(opt));
    return opt;
}
