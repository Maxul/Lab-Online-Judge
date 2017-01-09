#!/bin/bash

# only aided for Lab Online Judge

# core/judge.sh user_source.c problem_dir
if [ $# -ne 2 ]; then
	echo "$0 source_file problem_dir"
	exit 0
fi

# compose variable names
folder=`pwd`
folder=`dirname ${folder}/$0`
source=$1
problem=$2

# obtain base name with extension
name=`basename $source`
# strip suffix
name=${name%%.*}

# determine which compiler to exercise
suffix=${source##*.}

# try compiling the source file
case $suffix in
	c) clang -o $name -Wall -lm -std=c11 $source
	if [ 0 -ne $? ]; then
		status="Compile Error"
		echo $name , $status
		exit 0
	fi
	;;

	cpp) clang++ -o $name -Wall -std=c++11 $source
	if [ 0 -ne $? ]; then
		status="Compile Error"
		echo $name , $status
		exit 0
	fi
	;;
esac

# compiled successfully, execute it and compare the output
status=`$folder/JUDGE $name $problem`

# remove the binary executeable file
rm $name

# print the result and append it to destination
echo $name , $status | tee -a $problem/result
