#!/bin/bash

# only aided for Lab Online Judge

result=(
	"UN-USED"

	"Accepted"

	"Compile Error"

	"Memory Limit Exceeded"
	"Output Limit Exceeded"

	"Runtime Error"
	"System Error"

	"Time Limit Exceeded"

	"Presentation Error"
	"Wrong Anwser"
)

# OJ user_source.c testdata_directory
if [ $# -ne 2 ]; then
	echo "$0 source_file testdata_directory"
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
	;;
	cpp) clang++ -o $name -Wall -std=c++11 $source
	;;
	*) echo "Unsupported language type."
	;;
esac

if [ 0 -ne $? ]; then
	status="Compile Error"
fi

if [ -z "$status" ]; then
	# compiled successfully, execute it and compare the output
	in=`ls $problem/*.in`
	tmpfile=`openssl rand -hex 5`

	for infile in $in; do
		outfile=${infile%.*}.out
		$folder/SANDBOX $name < $infile > $tmpfile
		status=${result[$?]}
		
		# successfully pass the execute path
		if [ "$status" != ${result[1]} ]; then
			break
		else
			$folder/COMPARE $outfile $tmpfile
			status=${result[$?]}

			# drop hints for what's wrong
			if [ "$status" != ${result[1]} ]; then
				$folder/HINT $infile $outfile $tmpfile
				break
			fi
		fi
	done

	# remove the binary executeable file
	rm $name $tmpfile
fi

# print the result and append it to destination
echo -e "\n$name , $status" | tee -a $problem/result

