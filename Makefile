CFLAGS = -O2 -Wall -g -I./include

all: huffman rle

gentable: gentable.o

huffman: huffman.o gentable.o

rle: rle.o

.PHONY: clean
clean:
	rm gentable{,.o} huffman{,.o} rle{,.o}
