#!/bin/bash

code=`ls ../test/*.c`

for c in $code; do
	time ./OJ.sh $c ../test/data
done

#time ./OJ.sh ../test/CompileErr.c ../test/data

#time ./OJ.sh ../test/Accept.c ../test/data

#time ./OJ.sh ../test/TimeOut.c ../test/data
#time ./OJ.sh ../test/MemoryOut.c ../test/data

#time ./OJ.sh ../test/RuntimeErr.c ../test/data
#time ./OJ.sh ../test/WrongAnswer.c ../test/data
#time ./OJ.sh ../test/PreErr.c ../test/data
