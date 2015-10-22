# This file is part of MXE.
# See index.html for further information.

PKG             := nanomsg
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 0.6-beta
$(PKG)_CHECKSUM := 4c1016bac1df8464d05dad1bdb5106c190d36c22
$(PKG)_SUBDIR   := nanomsg-$($(PKG)_VERSION)
$(PKG)_FILE     := nanomsg-$($(PKG)_VERSION).tar.gz
$(PKG)_URL      := https://github.com/nanomsg/nanomsg/releases/download/0.6-beta/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc cmake pthreads autoconf automake

define $(PKG)_UPDATE
	echo 'TODO: Updates for package bdb48 need to be fixed.' >&2;
	echo $($(PKG)_VERSION)
endef

define $(PKG)_BUILD
    cd '$(1)' && autoreconf -ifv && ./configure \
	CC=$(PREFIX)/bin/$(TARGET)-gcc \
	--host=$(TARGET) \
	--prefix=$(PREFIX)/$(TARGET)

	$(MAKE) -C '$(1)' -j '$(JOBS)' LDFLAGS+='.libs/libnanomsg.a'
	$(MAKE) -C '$(1)' -j 1 install	
endef
