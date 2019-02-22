#!/bin/bash

# double-strncmp loop experiment
# generate increasingly long unrolled versions of the function
# 
# Usage:
#    scripts/experiments.sh | double_strncmp.csv 
#    python3 scripts/makeplot.py double_strncmp.csv iterations

cd counter
TEMPLATE=templates/double_strcmp.fmt
echo -n "iterations,hash,instructions,loads,stores,jumps,joins,"
echo zoneCrab?,zoneCrab_sec,zoneCrab_kb
for i in $(seq 1 68)
do
	BASE=$(basename $TEMPLATE)
	sed "s/VALUE_SIZE/$i/g" < $TEMPLATE > src/$BASE_$i.c
	make objects/$BASE_$i.o > /dev/null
	echo -n $i,$(../check objects/$BASE_$i.o --domain=stats),
	echo -n $(../check objects/$BASE_$i.o --domain=zoneCrab)
	rm -f objects/$BASE_$i.o src/$BASE_$i.c
	echo
done