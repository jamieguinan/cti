VPATH=..
CPPFLAGS=-I..
CPPFLAGS += $$(pkg-config --cflags serf-1) $$(pkg-config --cflags apr-1) $$(pkg-config --cflags apr-util-1)

%.o : %.c Makefile
	gcc $(CPPFLAGS) -Wall -O -c $<

filer-mirror: filer-mirror.o String.o serf_get.o Mem.o Regex.o
	gcc -o filer-mirror $^ \
	$$(pkg-config --libs serf-1) $$(pkg-config --libs apr-1) $$(pkg-config --libs apr-util-1)

# String.o Mem.o dpf.o Wav.o 
