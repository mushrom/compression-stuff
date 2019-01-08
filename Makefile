CFLAGS = -O2 -Wall -g -I./include

all: huffman rle lzs

gentable: gentable.o

huffman: huffman.o gentable.o

lzs: lzs.o

rle: rle.o

.PHONY: clean
clean:
	rm -f gentable{,.o} huffman{,.o} rle{,.o} lzs{,.o}
