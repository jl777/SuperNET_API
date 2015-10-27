/*
    Copyright (c) 2012-2013 Martin Sustrik  All rights reserved.
    Copyright (c) 2014 Achille Roussel All rights reserved.

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

#include "err.h"

#include <signal.h>
#include <stdint.h>

static void *nn_thread_main_routine(void *arg)
{
    struct nn_thread *self; nn_thread_routine *func;
    self = (struct nn_thread *)arg;
    //PostMessage("nn_thread_main_routine arg.%p self->routine(%p) at %p\n",arg,self->arg,self->routine);
    if ( (func= self->routine) != 0 )
    {
        func(self->arg); // Run the thread routine
    }
    return NULL;
}

void nn_thread_term(struct nn_thread *self)
{
    int32_t rc = 0;
    //PostMessage("Terminate thread.%p via pthread_join(%p)\n",self,self->handle);
    //printf("Terminate thread.%p via pthread_join(%p)\n",self,self->handle);
    //self->routine = 0;
    //pthread_exit(self->handle);
    rc = pthread_join(self->handle,NULL);
    //PostMessage("mark as Terminated thread.%p via pthread_join(%p) rc.%d\n",self,self->handle,rc);
    //printf("mark as Terminated thread.%p via pthread_join(%p) rc.%d\n",self,self->handle,rc);
    errnum_assert(rc == 0,rc);
}

#ifdef __PNACL
void nn_thread_init(struct nn_thread *self,nn_thread_routine *routine,void *arg)
{
    int32_t rc;
    // No signals should be processed by this thread. The library doesn't use signals and thus all the signals should be delivered to application threads, not to worker threads.
    //PostMessage("nn_thread_init routine.%p arg.%p\n",routine,arg);
    self->routine = routine;
    self->arg = arg;
    rc = pthread_create(&self->handle,NULL,nn_thread_main_routine,(void *)self);
    errnum_assert (rc == 0, rc);
}
#else

void nn_thread_init(struct nn_thread *self,nn_thread_routine *routine, void *arg)
{
    int32_t rc; sigset_t new_sigmask,old_sigmask; //pthread_attr_t attr;
   // No signals should be processed by this thread. The library doesn't use signals and thus all the signals should be delivered to application threads, not to worker threads.
    rc = sigfillset(&new_sigmask);
    errno_assert(rc == 0);
    rc = pthread_sigmask(SIG_BLOCK,&new_sigmask,&old_sigmask);
    errnum_assert(rc == 0, rc);
    self->routine = routine;
    self->arg = arg;
	//pthread_attr_init(&attr);
	//pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM);
	//pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
	//pthread_attr_setstacksize(&attr,256 * 1024);
    rc = pthread_create(&self->handle,NULL,nn_thread_main_routine,(void *)self);
    errnum_assert(rc == 0, rc);
   	//pthread_attr_destroy(&attr);
    //  Restore signal set to what it was before.
    rc = pthread_sigmask(SIG_SETMASK,&old_sigmask,NULL);
    errnum_assert(rc == 0, rc);
}
#endif

