#!/bin/sh

cat "$1" | ./rle -e | ./gentable > symbols.bin
cat "$1" | ./rle -e | ./huffman -e
