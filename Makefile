.PHONY: all
all: lc3sim lc3sim-trace

lc3sim: main.c lc3os.c vm.h vm.c
	gcc --std=c11 -o lc3sim *.c

lc3sim-trace: main.c lc3os.c vm.h vm.c
	gcc --std=c11 -o lc3sim-trace -DTRACE *.c

lc3os.c: lc3os.obj
	xxd -i lc3os.obj > lc3os.c
