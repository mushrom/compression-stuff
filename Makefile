CFLAGS = -O2 -Wall -g

all: gentable huffman rle

gentable: gentable.o

huffman: huffman.o

rle: rle.o

.PHONY: clean
clean:
	rm gentable{,.o} huffman{,.o} rle{,.o}
