/*
    Copyright (c) 2013 Martin Sustrik  All rights reserved.

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

#include "streamhdr.h"

#include "../../aio/timer.h"

#include "../../utils/err.h"
#include "../../utils/cont.h"
#include "../../utils/fast.h"
#include "../../utils/wire.h"
#include "../../utils/attr.h"

#include <stddef.h>
#include <string.h>

#define NN_STREAMHDR_STATE_IDLE 1
#define NN_STREAMHDR_STATE_SENDING 2
#define NN_STREAMHDR_STATE_RECEIVING 3
#define NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR 4
#define NN_STREAMHDR_STATE_STOPPING_TIMER_DONE 5
#define NN_STREAMHDR_STATE_DONE 6
#define NN_STREAMHDR_STATE_STOPPING 7

#define NN_STREAMHDR_SRC_USOCK 1
#define NN_STREAMHDR_SRC_TIMER 2

/*  Private functions. */
static void nn_streamhdr_handler(struct nn_fsm *self,int32_t src,int32_t type,void *srcptr);
static void nn_streamhdr_shutdown(struct nn_fsm *self,int32_t src,int32_t type,void *srcptr);

void nn_streamhdr_init(struct nn_streamhdr *self,int32_t src,struct nn_fsm *owner)
{
    //printf("nn_streamhdr_init\n");
    nn_fsm_init(&self->fsm,nn_streamhdr_handler,nn_streamhdr_shutdown,src,self,owner);
    self->state = NN_STREAMHDR_STATE_IDLE;
    nn_timer_init(&self->timer, NN_STREAMHDR_SRC_TIMER,&self->fsm);
    nn_fsm_event_init (&self->done);
    self->usock = NULL;
    self->usock_owner.src = -1;
    self->usock_owner.fsm = NULL;
    self->pipebase = NULL;
}

void nn_streamhdr_term(struct nn_streamhdr *self)
{
    //printf("nn_streamhdr_term\n");
    nn_assert_state(self,NN_STREAMHDR_STATE_IDLE);
    nn_fsm_event_term(&self->done);
    nn_timer_term(&self->timer);
    nn_fsm_term(&self->fsm);
}

int nn_streamhdr_isidle (struct nn_streamhdr *self) { return nn_fsm_isidle (&self->fsm); }

void nn_streamhdr_start(struct nn_streamhdr *self,struct nn_usock *usock,struct nn_pipebase *pipebase)
{
    size_t sz; int32_t protocol;
    // Take ownership of the underlying socket
    //printf("nn_streamhdr_start\n");
    nn_assert(self->usock == NULL && self->usock_owner.fsm == NULL);
    self->usock_owner.src = NN_STREAMHDR_SRC_USOCK;
    self->usock_owner.fsm = &self->fsm;
    nn_usock_swap_owner(usock,&self->usock_owner);
    self->usock = usock;
    self->pipebase = pipebase;
    sz = sizeof(protocol); // Get the protocol identifier
    nn_pipebase_getopt(pipebase,NN_SOL_SOCKET,NN_PROTOCOL,&protocol,&sz);
    nn_assert(sz == sizeof(protocol));
    memcpy(self->protohdr,"\0SP\0\0\0\0\0",8); // Compose the protocol header
    nn_puts(self->protohdr + 4,(uint16_t)protocol);
    nn_fsm_start(&self->fsm); // Launch the state machine
}

void nn_streamhdr_stop(struct nn_streamhdr *self)
{
    //printf("nn_streamhdr_stop\n");
    nn_fsm_stop(&self->fsm);
}

static void nn_streamhdr_shutdown(struct nn_fsm *self,int32_t src,int32_t type,NN_UNUSED void *srcptr)
{
    struct nn_streamhdr *streamhdr;
    streamhdr = nn_cont(self,struct nn_streamhdr,fsm);
    //printf("nn_streamhdr_shutdown\n");
    if ( nn_slow(src == NN_FSM_ACTION && type == NN_FSM_STOP) )
    {
        nn_timer_stop(&streamhdr->timer);
        streamhdr->state = NN_STREAMHDR_STATE_STOPPING;
    }
    if ( nn_slow(streamhdr->state == NN_STREAMHDR_STATE_STOPPING) )
    {
        if ( !nn_timer_isidle(&streamhdr->timer) )
            return;
        streamhdr->state = NN_STREAMHDR_STATE_IDLE;
        nn_fsm_stopped(&streamhdr->fsm,NN_STREAMHDR_STOPPED);
        return;
    }
    nn_fsm_bad_state(streamhdr->state, src, type);
}

static void nn_streamhdr_handler(struct nn_fsm *self,int32_t src,int32_t type,NN_UNUSED void *srcptr)
{
    struct nn_streamhdr *streamhdr; struct nn_iovec iovec; int32_t protocol;
    streamhdr = nn_cont(self,struct nn_streamhdr,fsm);
    //printf("nn_streamhdr_handler.(%d)\n",streamhdr->state);
    switch ( streamhdr->state )
    {
/******************************************************************************/
/*  IDLE state.                                                               */
/******************************************************************************/
    case NN_STREAMHDR_STATE_IDLE:
        switch (src)
        {
        case NN_FSM_ACTION:
            switch (type)
            {
            case NN_FSM_START:
                nn_timer_start(&streamhdr->timer, 1000);
                iovec.iov_base = streamhdr->protohdr;
                iovec.iov_len = sizeof(streamhdr->protohdr);
                nn_usock_send(streamhdr->usock,&iovec,1);
                //printf("NN_FSM_START send.%p[%d] (%d %d %d %d)\n",iovec.iov_base,(int32_t)iovec.iov_len,streamhdr->protohdr[0],streamhdr->protohdr[1],streamhdr->protohdr[2],streamhdr->protohdr[3]);
                streamhdr->state = NN_STREAMHDR_STATE_SENDING;
                return;
            default:
                nn_fsm_bad_action(streamhdr->state,src,type);
            }
        default: nn_fsm_bad_source(streamhdr->state,src,type);
        }

/******************************************************************************/
/*  SENDING state.                                                            */
/******************************************************************************/
    case NN_STREAMHDR_STATE_SENDING:
        switch ( src )
        {
        case NN_STREAMHDR_SRC_USOCK:
            switch ( type )
            {
            case NN_USOCK_SENT:
                nn_usock_recv(streamhdr->usock,streamhdr->protohdr,sizeof(streamhdr->protohdr),NULL);
                //printf("NN_USOCK_SENT received.(%d/%d/%d/%d)\n",streamhdr->protohdr[0],streamhdr->protohdr[1],streamhdr->protohdr[2],streamhdr->protohdr[3]);
                streamhdr->state = NN_STREAMHDR_STATE_RECEIVING;
                return;
            case NN_USOCK_SHUTDOWN: // Ignore it. Wait for ERROR event
                return;
            case NN_USOCK_ERROR:
                nn_timer_stop (&streamhdr->timer);
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR;
                return;
            default: nn_fsm_bad_action (streamhdr->state, src, type);
            }
        case NN_STREAMHDR_SRC_TIMER:
            switch ( type )
            {
            case NN_TIMER_TIMEOUT:
                //printf("NN_STREAMHDR_STATE_SENDING NN_STREAMHDR_SRC_TIMER timeout\n");
                nn_timer_stop(&streamhdr->timer);
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR;
                return;
            default: nn_fsm_bad_action(streamhdr->state, src, type);
            }
        default: nn_fsm_bad_source (streamhdr->state, src, type);
        }

/******************************************************************************/
/*  RECEIVING state.                                                          */
/******************************************************************************/
    case NN_STREAMHDR_STATE_RECEIVING:
        switch ( src )
        {
        case NN_STREAMHDR_SRC_USOCK:
            switch ( type )
            {
            case NN_USOCK_RECEIVED:
                //  Here we are checking whether the peer speaks the same protocol as this socket
                if ( memcmp(streamhdr->protohdr,"\0SP\0",4) != 0 )
                {
                    printf("protohdr miscompare (%d/%d/%d/%d):(%c/%c/%c/%c) vs (0/%d/%d/0)\n",streamhdr->protohdr[0],streamhdr->protohdr[1],streamhdr->protohdr[2],streamhdr->protohdr[3],streamhdr->protohdr[0],streamhdr->protohdr[1],streamhdr->protohdr[2],streamhdr->protohdr[3],'S','P');
                           goto invalidhdr;
                }
                protocol = nn_gets(streamhdr->protohdr + 4);
                if ( !nn_pipebase_ispeer(streamhdr->pipebase,protocol) )
                {
                    printf("pipebase rejection\n");
                    goto invalidhdr;
                }
                nn_timer_stop(&streamhdr->timer);
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_DONE;
                return;
            case NN_USOCK_SHUTDOWN: // Ignore it. Wait for ERROR event
                return;
            case NN_USOCK_ERROR:
invalidhdr:
                printf("NN_STREAMHDR_STATE_RECEIVING NN_STREAMHDR_SRC_USOCK invalid header\n");
                nn_timer_stop (&streamhdr->timer);
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR;
                return;
            default:
                nn_assert(0);
            }
        case NN_STREAMHDR_SRC_TIMER:
            switch ( type )
            {
            case NN_TIMER_TIMEOUT:
                //printf("NN_STREAMHDR_SRC_TIMER\n");
                nn_timer_stop(&streamhdr->timer);
                streamhdr->state = NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR;
                return;
            default:
                nn_fsm_bad_action (streamhdr->state, src, type);
            }
        default: nn_fsm_bad_source(streamhdr->state, src, type);
        }
/******************************************************************************/
/*  STOPPING_TIMER_ERROR state.                                               */
/******************************************************************************/
    case NN_STREAMHDR_STATE_STOPPING_TIMER_ERROR:
        switch ( src )
        {
        case NN_STREAMHDR_SRC_USOCK: // It's safe to ignore usock event when we are stopping
            return;
        case NN_STREAMHDR_SRC_TIMER:
            switch ( type )
            {
            case NN_TIMER_STOPPED:
                nn_usock_swap_owner(streamhdr->usock, &streamhdr->usock_owner);
                streamhdr->usock = NULL;
                streamhdr->usock_owner.src = -1;
                streamhdr->usock_owner.fsm = NULL;
                streamhdr->state = NN_STREAMHDR_STATE_DONE;
                nn_fsm_raise (&streamhdr->fsm, &streamhdr->done,
                    NN_STREAMHDR_ERROR);
                return;
            default:
                nn_fsm_bad_action (streamhdr->state, src, type);
            }

        default:
            nn_fsm_bad_source (streamhdr->state, src, type);
        }

/******************************************************************************/
/*  STOPPING_TIMER_DONE state.                                                */
/******************************************************************************/
    case NN_STREAMHDR_STATE_STOPPING_TIMER_DONE:
        switch (src) {

        case NN_STREAMHDR_SRC_TIMER:
            switch (type) {
            case NN_TIMER_STOPPED:
                nn_usock_swap_owner (streamhdr->usock, &streamhdr->usock_owner);
                streamhdr->usock = NULL;
                streamhdr->usock_owner.src = -1;
                streamhdr->usock_owner.fsm = NULL;
                streamhdr->state = NN_STREAMHDR_STATE_DONE;
                nn_fsm_raise (&streamhdr->fsm, &streamhdr->done,
                    NN_STREAMHDR_OK);
                return;
            default:
                nn_fsm_bad_action (streamhdr->state, src, type);
            }

        default:
            nn_fsm_bad_source (streamhdr->state, src, type);
        }

/******************************************************************************/
/*  DONE state.                                                               */
/*  The header exchange was either done successfully of failed. There's       */
/*  nothing that can be done in this state except stopping the object.        */
/******************************************************************************/
    case NN_STREAMHDR_STATE_DONE:
        nn_fsm_bad_source (streamhdr->state, src, type);

/******************************************************************************/
/*  Invalid state.                                                            */
/******************************************************************************/
    default:
        nn_fsm_bad_state (streamhdr->state, src, type);
    }
}

