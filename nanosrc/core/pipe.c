/*
    Copyright (c) 2012-2013 Martin Sustrik  All rights reserved.

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

#include "../transport.h"
#include "../protocol.h"

#include "sock.h"
#include "ep.h"

#include "../utils/err.h"
#include "../utils/fast.h"

/*  Internal pipe states. */
#define NN_PIPEBASE_STATE_IDLE 1
#define NN_PIPEBASE_STATE_ACTIVE 2
#define NN_PIPEBASE_STATE_FAILED 3

#define NN_PIPEBASE_INSTATE_DEACTIVATED 0
#define NN_PIPEBASE_INSTATE_IDLE 1
#define NN_PIPEBASE_INSTATE_RECEIVING 2
#define NN_PIPEBASE_INSTATE_RECEIVED 3
#define NN_PIPEBASE_INSTATE_ASYNC 4

#define NN_PIPEBASE_OUTSTATE_DEACTIVATED 0
#define NN_PIPEBASE_OUTSTATE_IDLE 1
#define NN_PIPEBASE_OUTSTATE_SENDING 2
#define NN_PIPEBASE_OUTSTATE_SENT 3
#define NN_PIPEBASE_OUTSTATE_ASYNC 4

void nn_pipebase_init(struct nn_pipebase *self,const struct nn_pipebase_vfptr *vfptr,struct nn_epbase *epbase)
{
    nn_assert(epbase->ep->sock);
    nn_fsm_init(&self->fsm,NULL,NULL,0,self,&epbase->ep->sock->fsm);
    self->vfptr = vfptr;
    self->state = NN_PIPEBASE_STATE_IDLE;
    self->instate = NN_PIPEBASE_INSTATE_DEACTIVATED;
    self->outstate = NN_PIPEBASE_OUTSTATE_DEACTIVATED;
    self->sock = epbase->ep->sock;
    memcpy(&self->options,&epbase->ep->options,sizeof(struct nn_ep_options));
    nn_fsm_event_init(&self->in);
    nn_fsm_event_init(&self->out);
}

void nn_pipebase_term(struct nn_pipebase *self)
{
    nn_assert_state(self,NN_PIPEBASE_STATE_IDLE);
    nn_fsm_event_term(&self->out);
    nn_fsm_event_term(&self->in);
    nn_fsm_term(&self->fsm);
}

int32_t nn_pipebase_start(struct nn_pipebase *self)
{
    int32_t rc;
    //printf("pipebase start.%p\n",self);
    nn_assert_state(self,NN_PIPEBASE_STATE_IDLE);
    self->state = NN_PIPEBASE_STATE_ACTIVE;
    self->instate = NN_PIPEBASE_INSTATE_ASYNC;
    self->outstate = NN_PIPEBASE_OUTSTATE_IDLE;
    rc = nn_sock_add(self->sock,(struct nn_pipe *)self);
    if ( nn_slow (rc < 0) )
    {
        self->state = NN_PIPEBASE_STATE_FAILED;
        return rc;
    }
    if ( self->sock )
        nn_fsm_raise(&self->fsm,&self->out,NN_PIPE_OUT);
    return 0;
}

void nn_pipebase_stop(struct nn_pipebase *self)
{
    //printf("pipebase stop.%p\n",self);
    if ( self->state == NN_PIPEBASE_STATE_ACTIVE )
        nn_sock_rm(self->sock,(struct nn_pipe *)self);
    self->state = NN_PIPEBASE_STATE_IDLE;
}

void nn_pipebase_received(struct nn_pipebase *self)
{
    //printf("nn_pipebase_received.%p\n",self);
    if ( nn_fast(self->instate == NN_PIPEBASE_INSTATE_RECEIVING) )
    {
        self->instate = NN_PIPEBASE_INSTATE_RECEIVED;
        return;
    }
    nn_assert(self->instate == NN_PIPEBASE_INSTATE_ASYNC);
    self->instate = NN_PIPEBASE_INSTATE_IDLE;
    if ( self->sock )
        nn_fsm_raise(&self->fsm,&self->in,NN_PIPE_IN);
}

void nn_pipebase_sent(struct nn_pipebase *self)
{
    //printf("nn_pipebase_sent.%p\n",self);
    if ( nn_fast(self->outstate == NN_PIPEBASE_OUTSTATE_SENDING) )
    {
        self->outstate = NN_PIPEBASE_OUTSTATE_SENT;
        return;
    }
    nn_assert(self->outstate == NN_PIPEBASE_OUTSTATE_ASYNC);
    self->outstate = NN_PIPEBASE_OUTSTATE_IDLE;
    if ( self->sock )
        nn_fsm_raise(&self->fsm,&self->out,NN_PIPE_OUT);
}

void nn_pipebase_getopt(struct nn_pipebase *self,int32_t level,int32_t option,void *optval,size_t *optvallen)
{
    int32_t rc,intval;
    if ( level == NN_SOL_SOCKET )
    {
        switch ( option ) // Endpoint options
        {
        case NN_SNDPRIO: intval = self->options.sndprio; break;
        case NN_RCVPRIO: intval = self->options.rcvprio; break;
        case NN_IPV4ONLY: intval = self->options.ipv4only; break;
        default: // Fallback to socket options
            rc = nn_sock_getopt_inner(self->sock,level,option,optval,optvallen);
            errnum_assert (rc == 0,-rc);
            return;
        }
        memcpy (optval,&intval,*optvallen < sizeof (int) ? *optvallen : sizeof (int));
        *optvallen = sizeof (int);
        return;
    }
    rc = nn_sock_getopt_inner(self->sock,level,option,optval,optvallen);
    errnum_assert (rc == 0, -rc);
}

int32_t nn_pipebase_ispeer(struct nn_pipebase *self,int32_t socktype)
{
    return nn_sock_ispeer(self->sock,socktype);
}

void nn_pipe_setdata(struct nn_pipe *self,void *data)
{
    //printf("nn_pipe_setdata(%p) data.%p\n",self,data);
    ((struct nn_pipebase *)self)->data = data;
}

void *nn_pipe_getdata(struct nn_pipe *self)
{
    //printf("nn_pipe_getdata(%p)\n",self);
    return((struct nn_pipebase *)self)->data;
}

int32_t nn_pipe_send(struct nn_pipe *self,struct nn_msg *msg)
{
    int32_t rc; struct nn_pipebase *pipebase;
    pipebase = (struct nn_pipebase *)self;
    nn_assert(pipebase->outstate == NN_PIPEBASE_OUTSTATE_IDLE);
    pipebase->outstate = NN_PIPEBASE_OUTSTATE_SENDING;
    //printf("%p nn_pipe_send.(%p) msg.%p\n",self,pipebase->vfptr->send,msg);
    rc = pipebase->vfptr->send(pipebase,msg);
    errnum_assert (rc >= 0, -rc);
    if ( nn_fast(pipebase->outstate == NN_PIPEBASE_OUTSTATE_SENT) )
    {
        pipebase->outstate = NN_PIPEBASE_OUTSTATE_IDLE;
        return rc;
    }
    nn_assert(pipebase->outstate == NN_PIPEBASE_OUTSTATE_SENDING);
    pipebase->outstate = NN_PIPEBASE_OUTSTATE_ASYNC;
    return rc | NN_PIPEBASE_RELEASE;
}

int32_t nn_pipe_recv(struct nn_pipe *self,struct nn_msg *msg)
{
    int32_t rc; struct nn_pipebase *pipebase;
    pipebase = (struct nn_pipebase *)self;
    nn_assert(pipebase->instate == NN_PIPEBASE_INSTATE_IDLE);
    pipebase->instate = NN_PIPEBASE_INSTATE_RECEIVING;
    //printf("nn_pipe_recv.(%p) msg.%p\n",self,msg);
    rc = pipebase->vfptr->recv(pipebase,msg);
    errnum_assert (rc >= 0, -rc);
    if ( nn_fast(pipebase->instate == NN_PIPEBASE_INSTATE_RECEIVED) )
    {
        pipebase->instate = NN_PIPEBASE_INSTATE_IDLE;
        return rc;
    }
    nn_assert(pipebase->instate == NN_PIPEBASE_INSTATE_RECEIVING);
    pipebase->instate = NN_PIPEBASE_INSTATE_ASYNC;
    return rc | NN_PIPEBASE_RELEASE;
}

void nn_pipe_getopt(struct nn_pipe *self,int32_t level,int32_t option,void *optval,size_t *optvallen)
{
    struct nn_pipebase *pipebase;
    pipebase = (struct nn_pipebase *)self;
    nn_pipebase_getopt(pipebase,level,option,optval,optvallen);
}

