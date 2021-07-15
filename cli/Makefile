CFLAGS += -Wall -Wpointer-arith -std=gnu99
CC := gcc
LDFLAGS +=
TARGETS = picorbc picoruby picoshell_lib/shell.c picoshell
MRUBYCDIR = ../src/mrubyc/src
PICORBCDIR = ../src

all: picorbc picoruby picoirb

picorbc: picorbc.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L../$(LIB_DIR) -lpicorbc

picoruby: picoruby.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L../$(LIB_DIR) -lmrubyc -lpicorbc

picoirb: picoirb.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L../$(LIB_DIR) -lmrubyc -lpicorbc

picoshell: picoshell.c heap.h picoshell_lib/shell.c picoshell_lib/shell.rb
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(filter-out picoshell_lib/shell.c picoshell_lib/shell.rb, $^) -L../$(LIB_DIR) -lmrubyc -lpicorbc

picoshell_lib/shell.c: picoshell_lib/shell.rb
	cd picoshell_lib ; make all

clean:
	cd picoshell_lib ; make clean
	rm -f $(TARGETS)
