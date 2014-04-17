benchmark="$1"

warns=$(grep "Write out-of-bound" cfg/MIT/$benchmark.warn | grep -v ": 00000000 : " | cut -f -4 -d ":" | sort -u | sed "s/ : /|/g" | cut -f 3 -d "|" | cut -f 2- -d "/" | sed "s/ //g" | sort -u )
bugs=$(cat BUGS | grep "^$benchmark" | cut -f 2 -d " ")

tmp1=$(mktemp)
tmp2=$(mktemp)

time=$(grep "### Wallclock time:" cfg/MIT/$benchmark.log | cut -f 2 -d ":")
mem=$(grep "### Maximum resident size" cfg/MIT/$benchmark.log | cut -f 5 -d " ")


RED="\x1b[31;01m"
GREEN="\x1b[32;01m"
YELLOW="\x1b[33;01m"
DEFAULT="\x1b[39;49;00m"

bugsno=0
for bug in $bugs
do
    echo $bug >> $tmp1
    bugsno=$((bugsno+1))
done

warnsno=0
fpno=0
fnno=0
for warn in $warns
do
    warnsno=$((warnsno+1))
    echo $warn >> $tmp2
    if grep -q $warn $tmp1
    then
        aaaaaaaaa=0
    else
        fpno=$((fpno+1))
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

benchmark=$(echo $benchmark | cut -f -2 -d "/" | sed "s/sendmail/Sendmail/" | sed "s/wu-ftpd/WU-FTPD/" | sed "s/bind/BIND/")

echo "$benchmark & $warnsno & $bugsno & $fpno & $fnno & $time & $mem \\\\"

rm -f $tmp1 $tmp2
