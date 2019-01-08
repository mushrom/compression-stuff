#!/bin/sh

temp=`mktemp`
./rle -e > $temp && ./huffman -e $temp
rm $temp
