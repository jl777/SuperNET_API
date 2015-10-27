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

#ifndef NNCONFIG_H_INCLUDED
#define NNCONFIG_H_INCLUDED

#define NN_HAVE_POLL 1 // must have
#define NN_HAVE_SEMAPHORE 1 // must have

// need one of following 3, listed in order of precedence, used by efd*
//#define NN_HAVE_EVENTFD 1
#define NN_HAVE_PIPE 1
//#define NN_HAVE_SOCKETPAIR 1

// need one of following 3, listed in order of precedence, used by poller*
#define NN_USE_POLL 1
//#define NN_USE_KQUEUE 1

#define NN_DISABLE_GETADDRINFO_A 1
#define NN_USE_LITERAL_IFADDR 1
#define NN_HAVE_STDINT 1

#define NN_HAVE_MSG_CONTROL 1


#ifdef __PNACL
void PostMessage(const char* format, ...);
#else
#define PostMessage(...)
#endif

#ifndef __PNACL
#define STANDALONE
#endif

#endif

