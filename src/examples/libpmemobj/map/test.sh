#!/bin/bash

PMEMBENCH_DIR=/media/ramdisk
#PMEMBENCH_DIR=.
NAME=10group
for i in `seq 10000 10000 50000`;
do
    for t in ctree;
    do
        file=$PMEMBENCH_DIR/$t
        if [ -f $file ] ; then
            rm $file
        fi

        OUTPUT=${i}_${NAME}_${t}.csv
        echo $file
        echo $OUTPUT
        ./mapcli $t $file 1461895662 10 $i | tee $OUTPUT
        rm $file

    done
done