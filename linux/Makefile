CC=clang
CFLAGS=-g

full: filetransfer.o linklayer_mod.o physical.o
	clang filetransfer.o linklayer_mod.o physical.o -o LLFT

test: LLtest.o linklayer_mod.o physical.o
	clang LLtest.o linklayer_mod.o physical.o -o LLTst

clean:
	rm -rf *.o
