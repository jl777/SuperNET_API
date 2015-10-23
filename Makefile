# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# GNU Makefile based on shared rules provided by the Native Client SDK.
# See README.Makefiles for more details.

#pepper_44/toolchain/mac_x86_newlib/x86_64-nacl/usr/lib

VALID_TOOLCHAINS := pnacl newlib glibc clang-newlib mac 

NACL_SDK_ROOT ?= $(abspath $(CURDIR))

TARGET = SuperNET_API


include $(NACL_SDK_ROOT)/tools/common.mk

CHROME_ARGS += --allow-nacl-socket-api=localhost

DEPS = nacl_io
LIBS = curl z pthread ssl crypto ppapi_simple ppapi nacl_io 
#LDFLAGS = -L $(NACL_SDK_ROOT)/toolchain/pnacl/usr/lib

CFLAGS = -Wall -Ipicocoin/include -D STANDALONE  -D __PNACL

#    nanosrc/core/ep.h \
#    nanosrc/core/global.h \
#    nanosrc/core/sock.h \

NANOMSG_CORE = \
    nanosrc/core/ep.c \
    nanosrc/core/epbase.c \
    nanosrc/core/global.c \
    nanosrc/core/pipe.c \
    nanosrc/core/poll.c \
    nanosrc/core/sock.c \
    nanosrc/core/sockbase.c \
    nanosrc/core/symbol.c

#    nanosrc/devices/device.h \

NANOMSG_DEVICES =\
    nanosrc/devices/device.c \
    nanosrc/devices/tcpmuxd.c

#   nanosrc/aio/poller_epoll.h \
#    nanosrc/aio/poller_epoll.c \
#    nanosrc/aio/poller_kqueue.h \
#    nanosrc/aio/poller_kqueue.c \
#    nanosrc/aio/poller_poll.c \

#    nanosrc/aio/usock_posix.h \
#    nanosrc/aio/usock_posix.c \
#    nanosrc/aio/usock.h \
#    nanosrc/aio/usock_win.h \
#    nanosrc/aio/usock_win.c \
#   nanosrc/aio/worker_win.h \
#    nanosrc/aio/worker_win.c
#    nanosrc/aio/fsm.h \
#    nanosrc/aio/poller.h \
#     nanosrc/aio/poller_poll.h \
#    nanosrc/aio/ctx.h \
#    nanosrc/aio/pool.h \
#    nanosrc/aio/timer.h \
#    nanosrc/aio/timerset.h \
#    nanosrc/aio/worker.h \
#    nanosrc/aio/worker_posix.h \
#    nanosrc/aio/worker_posix.c

NANOMSG_AIO = \
    nanosrc/aio/ctx.c \
    nanosrc/aio/fsm.c \
    nanosrc/aio/poller.c \
    nanosrc/aio/pool.c \
    nanosrc/aio/timer.c \
 nanosrc/aio/usock.c \
 nanosrc/aio/timerset.c \
    nanosrc/aio/worker.c \


#    nanosrc/utils/efd_eventfd.h \
#    nanosrc/utils/efd_eventfd.c \
#    nanosrc/utils/efd_pipe.h \
#    nanosrc/utils/efd_pipe.c \
#    nanosrc/utils/efd_win.h \
#    nanosrc/utils/efd_win.inc \
#    nanosrc/utils/thread_win.h \
#    nanosrc/utils/thread_win.c \
#    nanosrc/utils/efd_socketpair.c \
#    nanosrc/utils/thread_posix.c \
#    nanosrc/utils/thread_posix.h \
#    nanosrc/utils/win.h \
#    nanosrc/utils/attr.h \
#    nanosrc/utils/chunk.h \
#    nanosrc/utils/clock.h \
#    nanosrc/utils/chunkref.h \
#   nanosrc/utils/cont.h \
#    nanosrc/utils/efd.h \
#    nanosrc/utils/closefd.h \
#    nanosrc/utils/efd_socketpair.h \
#    nanosrc/utils/err.h \
#    nanosrc/utils/fast.h \
#    nanosrc/utils/fd.h \
#    nanosrc/utils/glock.h \
#    nanosrc/utils/atomic.h \
#    nanosrc/utils/hash.h \
#    nanosrc/utils/int.h \
#    nanosrc/utils/list.h \
#    nanosrc/utils/msg.h \
#    nanosrc/utils/mutex.h \
#    nanosrc/utils/queue.h \
#    nanosrc/utils/random.h \
#    nanosrc/utils/sem.h \
#    nanosrc/utils/sleep.h \
#    nanosrc/utils/stopwatch.h \
#    nanosrc/utils/thread.h \
#    nanosrc/utils/wire.h \
#    nanosrc/utils/alloc.h \

NANOMSG_UTILS = \
    nanosrc/utils/alloc.c \
    nanosrc/utils/atomic.c \
    nanosrc/utils/chunk.c \
    nanosrc/utils/chunkref.c \
    nanosrc/utils/clock.c \
    nanosrc/utils/closefd.c \
     nanosrc/utils/efd.c \
    nanosrc/utils/err.c \
    nanosrc/utils/glock.c \
    nanosrc/utils/hash.c \
    nanosrc/utils/list.c \
    nanosrc/utils/msg.c \
    nanosrc/utils/mutex.c \
    nanosrc/utils/queue.c \
    nanosrc/utils/random.c \
    nanosrc/utils/sem.c \
    nanosrc/utils/sleep.c \
    nanosrc/utils/stopwatch.c \
    nanosrc/utils/thread.c \
    nanosrc/utils/wire.c

#    nanosrc/protocols/utils/dist.h \
#    nanosrc/protocols/utils/excl.h \
#    nanosrc/protocols/utils/fq.h \
#    nanosrc/protocols/utils/lb.h \
#    nanosrc/protocols/utils/priolist.h \

PROTOCOLS_UTILS = \
    nanosrc/protocols/utils/dist.c \
    nanosrc/protocols/utils/excl.c \
    nanosrc/protocols/utils/fq.c \
    nanosrc/protocols/utils/lb.c \
    nanosrc/protocols/utils/priolist.c

#    nanosrc/protocols/bus/bus.h \
#    nanosrc/protocols/bus/xbus.h \

PROTOCOLS_BUS = \
    nanosrc/protocols/bus/bus.c \
    nanosrc/protocols/bus/xbus.c

#    nanosrc/protocols/pipeline/push.h \
#    nanosrc/protocols/pipeline/pull.h \
#    nanosrc/protocols/pipeline/xpull.h \
#    nanosrc/protocols/pipeline/xpush.h \

PROTOCOLS_PIPELINE = \
    nanosrc/protocols/pipeline/push.c \
    nanosrc/protocols/pipeline/pull.c \
    nanosrc/protocols/pipeline/xpull.c \
    nanosrc/protocols/pipeline/xpush.c

#    nanosrc/protocols/pair/pair.h \
#    nanosrc/protocols/pair/xpair.h \

PROTOCOLS_PAIR = \
    nanosrc/protocols/pair/pair.c \
    nanosrc/protocols/pair/xpair.c

#    nanosrc/protocols/pubsub/pub.h \
#    nanosrc/protocols/pubsub/sub.h \
#    nanosrc/protocols/pubsub/trie.h \
#    nanosrc/protocols/pubsub/xpub.h \
#    nanosrc/protocols/pubsub/xsub.h \

PROTOCOLS_PUBSUB = \
    nanosrc/protocols/pubsub/pub.c \
    nanosrc/protocols/pubsub/sub.c \
    nanosrc/protocols/pubsub/trie.c \
    nanosrc/protocols/pubsub/xpub.c \
    nanosrc/protocols/pubsub/xsub.c

#    nanosrc/protocols/reqrep/req.h \
#    nanosrc/protocols/reqrep/rep.h \
#    nanosrc/protocols/reqrep/task.h \
#    nanosrc/protocols/reqrep/xrep.h \
#    nanosrc/protocols/reqrep/xreq.h \

PROTOCOLS_REQREP = \
    nanosrc/protocols/reqrep/req.c \
    nanosrc/protocols/reqrep/rep.c \
    nanosrc/protocols/reqrep/task.c \
    nanosrc/protocols/reqrep/xrep.c \
    nanosrc/protocols/reqrep/xreq.c

#    nanosrc/protocols/survey/respondent.h \
#    nanosrc/protocols/survey/surveyor.h \
#    nanosrc/protocols/survey/xrespondent.h \
#    nanosrc/protocols/survey/xsurveyor.h \

PROTOCOLS_SURVEY = \
    nanosrc/protocols/survey/respondent.c \
    nanosrc/protocols/survey/surveyor.c \
    nanosrc/protocols/survey/xrespondent.c \
    nanosrc/protocols/survey/xsurveyor.c

NANOMSG_PROTOCOLS = \
    $(PROTOCOLS_BUS) \
    $(PROTOCOLS_PIPELINE) \
    $(PROTOCOLS_PAIR) \
    $(PROTOCOLS_PUBSUB) \
    $(PROTOCOLS_REQREP) \
    $(PROTOCOLS_SURVEY) \
    $(PROTOCOLS_UTILS)

#    nanosrc/transports/utils/dns_getaddrinfo.c \
#    nanosrc/transports/utils/dns_getaddrinfo_a.c \
#   nanosrc/transports/utils/dns_getaddrinfo.h \
#    nanosrc/transports/utils/dns_getaddrinfo_a.h \
#    nanosrc/transports/utils/backoff.h \
#    nanosrc/transports/utils/dns.h \
#     nanosrc/transports/utils/iface.h \
#    nanosrc/transports/utils/literal.h \
#    nanosrc/transports/utils/port.h \
#  nanosrc/transports/utils/streamhdr.h \
#    nanosrc/transports/utils/base64.h \

TRANSPORTS_UTILS = \
    nanosrc/transports/utils/backoff.c \
    nanosrc/transports/utils/dns.c \
    nanosrc/transports/utils/iface.c \
    nanosrc/transports/utils/literal.c \
    nanosrc/transports/utils/port.c \
  nanosrc/transports/utils/streamhdr.c \
    nanosrc/transports/utils/base64.c

#    nanosrc/transports/inproc/binproc.h \
#    nanosrc/transports/inproc/cinproc.h \
#    nanosrc/transports/inproc/inproc.h \
#    nanosrc/transports/inproc/ins.h \
#    nanosrc/transports/inproc/msgqueue.h \
#    nanosrc/transports/inproc/sinproc.h \

TRANSPORTS_INPROC = \
    nanosrc/transports/inproc/binproc.c \
    nanosrc/transports/inproc/cinproc.c \
    nanosrc/transports/inproc/inproc.c \
    nanosrc/transports/inproc/ins.c \
    nanosrc/transports/inproc/msgqueue.c \
    nanosrc/transports/inproc/sinproc.c

#    nanosrc/transports/ipc/aipc.h \
#    nanosrc/transports/ipc/bipc.h \
#    nanosrc/transports/ipc/cipc.h \
#    nanosrc/transports/ipc/ipc.h \
#    nanosrc/transports/ipc/sipc.h \

TRANSPORTS_IPC = \
    nanosrc/transports/ipc/aipc.c \
    nanosrc/transports/ipc/bipc.c \
    nanosrc/transports/ipc/cipc.c \
    nanosrc/transports/ipc/ipc.c \
    nanosrc/transports/ipc/sipc.c

#    nanosrc/transports/tcp/atcp.h \
#    nanosrc/transports/tcp/btcp.h \
#    nanosrc/transports/tcp/ctcp.h \
#    nanosrc/transports/tcp/stcp.h \
#    nanosrc/transports/tcp/tcp.h \

TRANSPORTS_TCP = \
    nanosrc/transports/tcp/atcp.c \
    nanosrc/transports/tcp/btcp.c \
    nanosrc/transports/tcp/ctcp.c \
    nanosrc/transports/tcp/stcp.c \
    nanosrc/transports/tcp/tcp.c

#    nanosrc/transports/ws/aws.h \
#    nanosrc/transports/ws/bws.h \
#    nanosrc/transports/ws/cws.h \
#    nanosrc/transports/ws/sws.h \
#    nanosrc/transports/ws/ws.h \
#    nanosrc/transports/ws/ws_handshake.h \
#    nanosrc/transports/ws/sha1.h \

TRANSPORTS_WS = \
    nanosrc/transports/ws/aws.c \
    nanosrc/transports/ws/bws.c \
    nanosrc/transports/ws/cws.c \
    nanosrc/transports/ws/sws.c \
    nanosrc/transports/ws/ws.c \
    nanosrc/transports/ws/ws_handshake.c \
    nanosrc/transports/ws/sha1.c

#    nanosrc/transports/tcpmux/atcpmux.h \
#    nanosrc/transports/tcpmux/btcpmux.h \
#    nanosrc/transports/tcpmux/ctcpmux.h \
#    nanosrc/transports/tcpmux/stcpmux.h \
#    nanosrc/transports/tcpmux/tcpmux.h \

TRANSPORTS_TCPMUX = \
    nanosrc/transports/tcpmux/atcpmux.c \
    nanosrc/transports/tcpmux/btcpmux.c \
    nanosrc/transports/tcpmux/ctcpmux.c \
    nanosrc/transports/tcpmux/stcpmux.c \
    nanosrc/transports/tcpmux/tcpmux.c


NANOMSG_TRANSPORTS =  $(TRANSPORTS_UTILS)  $(TRANSPORTS_INPROC)  $(TRANSPORTS_TCP)  $(TRANSPORTS_TCPMUX) $(TRANSPORTS_IPC) $(TRANSPORTS_WS)

#    nanosrc/transport.h \
#    nanosrc/protocol.h \

libnanomsg_la_SOURCES = \
  $(NANOMSG_CORE) \
  $(NANOMSG_AIO) \
  $(NANOMSG_UTILS) \
  $(NANOMSG_DEVICES) \
  $(NANOMSG_PROTOCOLS) \
  $(NANOMSG_TRANSPORTS)

################################################################################
#  automated tests                                                             #
################################################################################

TRANSPORT_TESTS = \
    nanosrc/tests/inproc.c  \
    nanosrc/tests/inproc_shutdown.c  \
    nanosrc/tests/ipc.c  \
    nanosrc/tests/ipc_shutdown.c  \
    nanosrc/tests/ipc_stress.c  \
    nanosrc/tests/tcp.c  \
    nanosrc/tests/tcp_shutdown.c  \
    nanosrc/tests/ws.c  \
    nanosrc/tests/tcpmux.c

PROTOCOL_TESTS = \
    nanosrc/tests/pair.c  \
    nanosrc/tests/pubsub.c \
    nanosrc/tests/reqrep.c  \
    nanosrc/tests/pipeline.c  \
    nanosrc/tests/survey.c  \
    nanosrc/tests/bus.c

FEATURE_TESTS = \
    nanosrc/tests/block.c  \
    nanosrc/tests/term.c  \
    nanosrc/tests/timeo.c  \
    nanosrc/tests/iovec.c  \
    nanosrc/tests/msg.c  \
    nanosrc/tests/prio.c  \
    nanosrc/tests/poll.c \
    nanosrc/tests/device.c  \
    nanosrc/tests/emfile.c  \
    nanosrc/tests/domain.c  \
    nanosrc/tests/trie.c  \
    nanosrc/tests/list.c  \
    nanosrc/tests/hash.c  \
    nanosrc/tests/symbol.c  \
    nanosrc/tests/separation.c  \
    nanosrc/tests/zerocopy.c  \
    nanosrc/tests/shutdown.c  \
    nanosrc/tests/cmsg.c

check_PROGRAMS = \
    $(TRANSPORT_TESTS) \
    $(PROTOCOL_TESTS) \
    $(FEATURE_TESTS)

TESTS = $(check_PROGRAMS)

SOURCES = handlers.c \
  $(libnanomsg_la_SOURCES) \
  nacl_io_demo.c \
  plugins/coins/bitcoind_RPC.c \
  picocoin/base58.c \
  picocoin/key.c \
  picocoin/bignum.c \
  picocoin/file_seq.c \
  picocoin/clist.c \
  picocoin/util.c \
  picocoin/cstr.c \
  SuperNET.c \
  libjl777.c \
  plugins/nonportable/newos/random.c \
  plugins/nonportable/newos/files.c \
  plugins/peggy/quotes777.c \
  plugins/peggy/peggy777.c \
  plugins/coins/coins777_main.c \
  plugins/coins/coins777.c \
  plugins/coins/gen1.c \
  plugins/coins/cointx.c \
  plugins/InstantDEX/InstantDEX_main.c \
  plugins/KV/kv777.c \
  plugins/KV/ramkv777.c \
  plugins/KV/kv777_main.c \
  plugins/agents/shuffle777.c \
  plugins/agents/_dcnet/dcnet777.c \
  plugins/utils/ramcoder.c \
  plugins/utils/inet.c \
  plugins/utils/curve25519.c \
  plugins/utils/crypt_argchk.c \
  plugins/utils/tom_md5.c \
  plugins/utils/transport777.c \
  plugins/utils/sha256.c \
  plugins/utils/rmd160.c \
  plugins/utils/hmac_sha512.c \
  plugins/utils/sha512.c \
  plugins/utils/NXT777.c \
  plugins/utils/bits777.c \
  plugins/utils/utils777.c \
  plugins/utils/cJSON.c \
  plugins/utils/huffstream.c \
  plugins/utils/libgfshare.c \
  plugins/utils/SaM.c \
  plugins/utils/files777.c \
  plugins/games/tourney777.c \
  plugins/games/cards777.c \
  plugins/games/pangea/pangea777.c \
  plugins/games/pangea/poker.c \
  plugins/common/teleport777.c \
  plugins/common/console777.c \
  plugins/common/busdata777.c \
  plugins/common/relays777.c \
  plugins/common/hostnet777.c \
  plugins/common/system777.c \
  plugins/common/txind777.c \
  plugins/common/opreturn777.c \
  plugins/common/prices777.c \
  queue.c \
  $(TESTS)


# Build rules generated by macros from common.mk:

$(foreach dep,$(DEPS),$(eval $(call DEPEND_RULE,$(dep))))
$(foreach src,$(SOURCES),$(eval $(call COMPILE_RULE,$(src),$(CFLAGS))))

# The PNaCl workflow uses both an unstripped and finalized/stripped binary.
# On NaCl, only produce a stripped binary for Release configs (not Debug).
ifneq (,$(or $(findstring pnacl,$(TOOLCHAIN)),$(findstring Release,$(CONFIG))))
$(eval $(call LINK_RULE,$(TARGET)_unstripped,$(SOURCES),$(LIBS),$(DEPS)))
$(eval $(call STRIP_RULE,$(TARGET),$(TARGET)_unstripped))
else
$(eval $(call LINK_RULE,$(TARGET),$(SOURCES),$(LIBS),$(DEPS)))
endif

$(eval $(call NMF_RULE,$(TARGET),))
