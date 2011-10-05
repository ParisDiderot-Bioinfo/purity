# makefile for the purity test
#
# edit all appropriate paths, and then make
#
# make sure the definitions in pt.h are correct before making


# BINDIR: the directory where the binary will go.
BINDIR = /usr/games

#   LIBDIR: the directory where the test datafiles are kept
LIBDIR = /usr/share/games/purity

# options for the c compiler (-g, -O, etc.)
# Specify -DSYS for sysV, -DLINUX for linux, BSD is default
#CFLAGS = -O
CFLAGS = -O6 -s -DLINUX

# compiler libraries (-ltermcap, etc.)
LIBS =

purity: pt.c pt.h
	cc $(CFLAGS) -DLIBDIR=\"$(LIBDIR)\" -o purity pt.c $(LIBS)
clean:
	rm purity
install: purity
	mkdir -p ${DESTDIR}/usr/games
	install purity ${DESTDIR}/usr/games
	mkdir -p ${DESTDIR}/usr/share/games/purity
	cp -r tests/* ${DESTDIR}/usr/share/games/purity
