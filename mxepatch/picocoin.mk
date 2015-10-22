# This file is part of MXE.
# See index.html for further information.

PKG             := picocoin
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 0.1
$(PKG)_CHECKSUM := 63624a38b93ff491f143469f601d7815d66e5d9e
$(PKG)_SUBDIR   := picocoin-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_VERSION).tar.gz
$(PKG)_URL      := https://github.com/mezzovide/picocoin/archive/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc cmake autoconf automake jansson libevent openssl

define $(PKG)_UPDATE
	echo 'TODO: Updates for package picocoin need to be fixed.' >&2;
	echo $($(PKG)_VERSION)
endef

define $(PKG)_BUILD
    cd '$(1)' && autoreconf -ifv && ./configure \
	CC=$(PREFIX)/bin/$(TARGET)-gcc \
	--host=$(TARGET) \
	--prefix=$(PREFIX)/$(TARGET)
 
    $(MAKE) -C '$(1)/lib' -j '$(JOBS)' && cp $(1)/lib/libccoin.a $(PREFIX)/$(TARGET)/lib/libccoin.a
endef
