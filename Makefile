exec_prefix = ${prefix}
prefix = /usr/local
bindir = ${exec_prefix}/bin
sysconfdir = ${prefix}/etc
localstatedir = ${prefix}/var

CC = gcc
LDFLAGS =  -L/usr/local/lib -L/usr/local/lib -L/usr/local/lib/mysql -lmysqlclient  -pthread -lz -lm -lmysqlclient -lpcap -lpng -liconv -lz -lm 
OBS= bandwidthd.o graph.o conf.tab.o conf.l.o

CFLAGS= -Wall -g -O2 -g -O2 -I/usr/local/include/mysql -DHAVE_CONFIG_H
NONWALLCFLAGS= -g -O2 -g -O2 -I/usr/local/include/mysql -DHAVE_CONFIG_H

# Debugging stuff
#CFLAGS += -Wall -pg -DPROFILE
#CFLAGS += -Wall -g -DDEBUG 

all: bandwidthd

bandwidthd: $(OBS) bandwidthd.h
	$(CC) $(CFLAGS) $(OBS) -o bandwidthd $(LDFLAGS) 

conf.tab.c: conf.y
	bison -y -pbdconfig_ -d conf.y
	mv y.tab.c conf.tab.c
	mv y.tab.h conf.tab.h

conf.l.c: conf.l
	flex -Pbdconfig_ -s -i -t -I conf.l > conf.l.c

clean:
	-rm -f *.o bandwidthd *~ DEADJOE core

# Resets us to distributable code
distclean: conf.tab.c conf.l.c clean
	@echo "Deleting all files that don't come with the distribution"
	-rm -f config.h config.log config.status Makefile

# Deletes all derived files
# You must have flex, bison, and autoconf to build the source if you run this
# Before you can do anything after running this you must run "autoheader; autoconf"
real-clean: distclean
	@echo 'Deleting all derived files, you will need to run'
	@echo 'autoconf && autoheader && ./configure && make'
	@echo 'to build the package'
	-rm -f conf.tab.c conf.tab.h conf.l.c config.h.in configure 

install: all
	/usr/bin/install -c -d $(DESTDIR)$(bindir)
	/usr/bin/install -c -d $(DESTDIR)$(sysconfdir)
	/usr/bin/install -c -d $(DESTDIR)$(localstatedir)/bandwidthd/htdocs
	/usr/bin/install -c -m755 -s bandwidthd $(DESTDIR)$(bindir)	
	if [ ! -f $(DESTDIR)$(sysconfdir)/bandwidthd.conf ] ; then /usr/bin/install -c -m644 etc/bandwidthd.conf $(DESTDIR)$(sysconfdir); fi
	/usr/bin/install -c -m644 htdocs/legend.png $(DESTDIR)$(localstatedir)/bandwidthd/htdocs
	/usr/bin/install -c -m644 htdocs/logo.gif $(DESTDIR)$(localstatedir)/bandwidthd/htdocs

#**** Stuff where -WALL is turned off to reduce the noise in a compile so I can see my own errors *******************
conf.l.o: conf.l.c
	$(CC) $(NONWALLCFLAGS) -c -o conf.l.o conf.l.c	
