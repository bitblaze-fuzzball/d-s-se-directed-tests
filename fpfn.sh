benchmark="$1"

warns=$(grep "Write out-of-bound" cfg/MIT/$benchmark.warn | grep -v ": 00000000 : " | cut -f -4 -d ":" | sort -u | sed "s/ : /|/g" | cut -f 3 -d "|" | cut -f 2- -d "/" | sed "s/ //g")
bugs=$(cat BUGS | grep "^$benchmark" | cut -f 2 -d " ")

tmp1=$(mktemp)
tmp2=$(mktemp)

RED="\x1b[31;01m"
GREEN="\x1b[32;01m"
YELLOW="\x1b[33;01m"
DEFAULT="\x1b[39;49;00m"

for bug in $bugs
do
    echo $bug >> $tmp1
done

for warn in $warns
do
    echo $warn >> $tmp2
    if grep -q $warn $tmp1
    then
	echo -ne "\t$GREEN"
	echo -n "$warn"
	echo -e "$DEFAULT"
    else
	echo -ne "\t$YELLOW"
	echo -n "$warn (false positive)"
	echo -e "$DEFAULT"
    fi
done

for bug in $bugs
do
    if grep -q $bug $tmp2
    then
	echo -n ""
    else
	echo -ne "\t$RED"
	echo -n "$bug (false negative)"
	echo -e "$DEFAULT"
    fi
done

rm -f $tmp1 $tmp2