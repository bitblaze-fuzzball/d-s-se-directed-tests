#!/bin/bash

tmp=$(mktemp)
PIN=./pin-2.14-71313-gcc.4.4.7-linux/pin
OUTDIR="./cfg"
DEBUG="3"

while getopts "d:sO:o:c:" flag
do
    case $flag in
	O) OUTDIR="$OPTARG";;
	o) OUTFILE="$OPTARG";;
	c) CHDIR="--chdir=$OPTARG";;
	s) STATIC="--augment-cfg";;
	d) DEBUG="$OPTARG";;
    esac
done

shift $((OPTIND-1))

if [ "$*" == "" ]
then
    echo "Invalid argument(s)"
    exit 1
else
    PT_ARGS="--skiplibs=0 --debug=$DEBUG $CHDIR --outprog=$tmp $STATIC"
    PROG="$(which $1)"
    shift 1
    export LD_BIND_NOW=1
    $PIN -ifeellucky -injection child -t pintracer.so $PT_ARGS -- "$PROG" "$@"
    ret=$?

    if [ $ret -eq 0 ]
    then
	MD51=$(md5sum "$PROG" | cut -f 1 -d " " | cut -c -6)
	MD52=$(md5sum "$tmp" | cut -f 1 -d " " | cut -c -6)
	PROG=$(basename "$PROG")
	if [ "$STATIC" = "" ]
	then
	    EXT="dyn.cfg"
	else
	    EXT="stat.cfg"
	fi
	if [ "$OUTFILE" = "" ]
	then
	    OUTFILE="$OUTDIR/$PROG-$MD51-$MD52.$EXT"
	fi
	mv -f "$tmp" "$OUTFILE"
	echo "**** Program succesfully executed (trace saved in $OUTFILE)"
    else
	echo "!!!! Execution failed"
    fi

    rm -f $CFG
fi
