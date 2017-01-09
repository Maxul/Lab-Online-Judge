#!/bin/bash

if [ $# -ne 4 ]; then
	echo "Usage: $0 fileNO lineNO testGenTool.c src.c"
	exit 0
fi

cnt=$1
line=$2
gen=$3
src=$4

# compile input generate source code
gcc $gen -o Gen_Unit -Wall

# compile programe source code
gcc $src -o Src_Code -Wall

for ((i=0; i<$cnt; ++i)); do
	rm $i.in 2>>/dev/null
	./Gen_Unit $line >> $i.in
#	for ((j=0; j<$line; ++j)); do $gen >> $i.in done
	./Src_Code < $i.in > $i.out
done

exit 0

