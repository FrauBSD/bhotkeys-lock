############################################################ LICENSE
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
#
############################################################ IDENT(1)
#
# $Title: bhotkeys-lock - Super+L xlock-screen $
# $Copyright: 2026 Devin Teske. All rights reserved. $
# $FrauBSD: bhotkeys-lock/Makefile 2026-10-04 07:53:53 -0700 Devin Teske $
#
############################################################ PATHS

PREFIX?=	/usr/local
BINDIR?=	${PREFIX}/bin
PLUGDIR?=	${PREFIX}/share/bhotkeys/plugins.d

############################################################ PKG-CONFIG

PKG_CONFIG?=	pkg-config
PKGS=		x11 xext
PKG_CFLAGS!=	${PKG_CONFIG} --cflags ${PKGS}
PKG_LIBS!=	${PKG_CONFIG} --libs ${PKGS}

############################################################ COMPILER

CC?=		cc
CFLAGS?=	-O2 -Wall -Wextra

############################################################ FILES

BIN=		bin/xlock-screen bin/xlock-invoke bin/xlock-run
PLUG=		plugins.d/lock

############################################################ TARGETS

.PHONY: all

all: bin/xlock-run

bin/xlock-run: src/xlock-run.c
	${CC} ${CFLAGS} ${PKG_CFLAGS} -o ${.TARGET} src/xlock-run.c ${PKG_LIBS}

.PHONY: install

install: all
	mkdir -p ${DESTDIR}${BINDIR} ${DESTDIR}${PLUGDIR}
	install -m 755 ${BIN} ${DESTDIR}${BINDIR}
	install -m 644 ${PLUG} ${DESTDIR}${PLUGDIR}/lock

.PHONY: clean

clean:
	rm -f bin/xlock-run

################################################################################
# END
################################################################################
