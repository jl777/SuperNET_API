/*
    Copyright (c) 2012 Martin Sustrik  All rights reserved.

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
#include "../utils/err.h"

#define NN_POLLER_GRANULARITY 16

int nn_poller_init(struct nn_poller *self)
{
    self->size = 0;
    self->index = 0;
    self->capacity = NN_POLLER_GRANULARITY;
    self->pollset = nn_alloc(sizeof(struct pollfd) * NN_POLLER_GRANULARITY,"pollset");
    alloc_assert (self->pollset);
    self->hndls = nn_alloc(sizeof(struct nn_hndls_item) * NN_POLLER_GRANULARITY,"hndlset");
    alloc_assert(self->hndls);
    self->removed = -1;
    return 0;
}

void nn_poller_term(struct nn_poller *self)
{
    //printf("nn_poller_term\n");
    nn_free(self->pollset);
    nn_free(self->hndls);
}

void nn_poller_add(struct nn_poller *self,int32_t fd,struct nn_poller_hndl *hndl)
{
    // If the capacity is too low to accommodate the next item, resize it
    //printf("nn_poller_add fd.%d hndl.%p size.%d\n",fd,hndl,self->size);
    if ( nn_slow(self->size >= self->capacity) )
    {
        self->capacity *= 2;
        self->pollset = nn_realloc(self->pollset,sizeof(struct pollfd) * self->capacity);
        alloc_assert(self->pollset);
        self->hndls = nn_realloc (self->hndls,sizeof(struct nn_hndls_item) * self->capacity);
        alloc_assert(self->hndls);
    }
    // Add the fd to the pollset
    self->pollset[self->size].fd = fd;
    self->pollset[self->size].events = 0;
    self->pollset[self->size].revents = 0;
    hndl->index = self->size;
    self->hndls[self->size].hndl = hndl;
    ++self->size;
}

void nn_poller_rm(struct nn_poller *self, struct nn_poller_hndl *hndl)
{
 //   printf("nn_poller_rm hndl.%p size.%d\n",hndl,self->size);
    //  No more events will be reported on this fd
    self->pollset[hndl->index].revents = 0;
    // Add the fd into the list of removed fds
    if ( self->removed != -1 )
        self->hndls[self->removed].prev = hndl->index;
    self->hndls[hndl->index].hndl = NULL;
    self->hndls[hndl->index].prev = -1;
    self->hndls[hndl->index].next = self->removed;
    self->removed = hndl->index;
}

void nn_poller_set_in(struct nn_poller *self,struct nn_poller_hndl *hndl)
{
    //printf("nn_poller_set_in index.%d size.%d %p\n",hndl->index,self->size,&self->pollset[hndl->index].events);
    self->pollset[hndl->index].events |= POLLIN;
}

void nn_poller_reset_in(struct nn_poller *self,struct nn_poller_hndl *hndl)
{
    self->pollset[hndl->index].events &= ~POLLIN;
    self->pollset[hndl->index].revents &= ~POLLIN;
}

void nn_poller_set_out(struct nn_poller *self,struct nn_poller_hndl *hndl)
{
    self->pollset[hndl->index].events |= POLLOUT;
}

void nn_poller_reset_out(struct nn_poller *self,struct nn_poller_hndl *hndl)
{
    self->pollset[hndl->index].events &= ~POLLOUT;
    self->pollset[hndl->index].revents &= ~POLLOUT;
}

int32_t nn_poller_wait(struct nn_poller *self,int32_t timeout)
{
    int32_t i,rc = 0;
    // First, get rid of removed fds
    while ( self->removed != -1 )
    {
        // Remove the fd from the list of removed fds
        i = self->removed;
        self->removed = self->hndls[i].next;
        // Replace the removed fd by the one at the end of the pollset
        --self->size;
        //printf("decrement in nn_poller_wait i.%d vs size.%d\n",i,self->size);
        if ( i != self->size )
        {
            self->pollset[i] = self->pollset[self->size];
            if (self->hndls[i].next != -1)
                self->hndls[self->hndls[i].next].prev = -1;
            self->hndls[i] = self->hndls[self->size];
            if (self->hndls[i].hndl)
                self->hndls[i].hndl->index = i;
        }
        // The fd from the end of the pollset may have been on removed fds list itself. If so, adjust the removed list
        if ( nn_slow( !self->hndls [i].hndl) )
        {
            if (self->hndls[i].prev != -1)
               self->hndls[self->hndls[i].prev].next = i;
            if (self->hndls[i].next != -1)
               self->hndls[self->hndls[i].next].prev = i;
            if (self->removed == self->size)
                self->removed = i;
        }
    }
    self->index = 0;
    // Wait for new events
#if defined NN_IGNORE_EINTR
again:
#endif
    //if ( timeout > 0 )
    {
        //printf("nn_poller_wait timeout.%d\n",timeout);
        rc = poll(self->pollset,self->size,timeout);
        //printf("nn_poller got rc.%d\n",rc);
        if ( nn_slow (rc < 0 && errno == EINTR) )
#if defined NN_IGNORE_EINTR
            goto again;
#else
        return -EINTR;
#endif
    }
    errno_assert(rc >= 0);
    return 0;
}

int nn_poller_event(struct nn_poller *self,int32_t *event,struct nn_poller_hndl **hndl)
{
    // Skip over empty events. This will also skip over removed fds as they have their revents nullified
    while ( self->index < self->size )
    {
        //printf("self->index %d vs %d self->size | fd.%d revents %d\n",self->index,self->size,self->pollset[self->index].fd,self->pollset[self->index].revents);
        if ( self->pollset[self->index].revents != 0 )
        {
            //printf("BREAK\n");
            break;
        }
        ++self->index;
    }
    // If there is no available event, let the caller know
    if ( nn_slow(self->index >= self->size) )
    {
        //printf("nn_poller_event: self->index %d >= %d self->size\n",self->index,self->size);
        return -EAGAIN;
    }
    // Return next event to the caller. Remove the event from revents
    *hndl = self->hndls[self->index].hndl;
    if ( nn_fast(self->pollset[self->index].revents & POLLIN) )
    {
        //printf("NN_POLLER_IN\n");
        *event = NN_POLLER_IN;
        self->pollset[self->index].revents &= ~POLLIN;
        return 0;
    }
    else if ( nn_fast(self->pollset[self->index].revents & POLLOUT) )
    {
        //printf("NN_POLLER_OUT\n");
        *event = NN_POLLER_OUT;
        self->pollset[self->index].revents &= ~POLLOUT;
        return 0;
    }
    else
    {
        printf("NN_POLLER_ERR\n");
        *event = NN_POLLER_ERR;
        ++self->index;
        return 0;
    }
}

