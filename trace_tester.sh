#!/bin/bash

touch diffs

./tester -w traces/simple-input -s 4096 >x
t1=$(diff x traces/simple-expected-output >diffs)

if [ -s diffs ]; then
	echo simple input failed
	exit
fi

./tester -w traces/linear-input -s 4096 >x
t2=$(diff x traces/linear-expected-output >diffs)

if [ -s diffs ]; then
	echo linear input failed
	exit
fi

./tester -w traces/random-input -s 4096 >x
t3=$(diff x traces/random-expected-output >diffs)

if [ -s diffs ]; then
	echo random input failed
	exit
fi

#./tester -w traces/scan-input -s 4096 >x
#t4=$(diff x traces/scan-expected-output >diffs)

#if [ -s diffs ]; then
#	echo scan input failed
#	exit
#fi

echo all tests passed!
