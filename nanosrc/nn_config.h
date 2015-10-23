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

#ifdef __APPLE__
#define NN_HAVE_OSX 1
#endif

#define NN_HAVE_GCC 1
#define NN_HAVE_CLANG 1
#define HAVE_PTHREAD_PRIO_INHERIT 1
#define STDC_HEADERS 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NETDB_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_STDINT_H 1
#define HAVE_ARPA_INET_H 1
#define HAVE_STDINT_H 1
#define HAVE_PIPE 1
#define HAVE_POLL 1
#define HAVE_KQUEUE 1
#define HAVE_GETIFADDRS 1
#define HAVE_DLFCN_H 1

#define NN_HAVE_STDINT 1
#define NN_HAVE_PIPE 1
#define NN_HAVE_POLL 1
#define NN_HAVE_KQUEUE 1
#define NN_HAVE_SOCKETPAIR 1
#define NN_HAVE_SEMAPHORE 1
#define NN_HAVE_MSG_CONTROL 1

//#define NN_HAVE_EVENTFD 1

//#define NN_USE_KQUEUE 1
#define NN_USE_POLL 1
//#define NN_USE_EPOLL 1
#define NN_USE_PIPE 1

#define NN_DISABLE_GETADDRINFO_A 1
#define NN_USE_LITERAL_IFADDR 1

#ifdef __PNACL
void PostMessage(const char* format, ...);
#else
#define PostMessage(...)
#endif

#ifndef __PNACL
#define STANDALONE
#endif

#endif

