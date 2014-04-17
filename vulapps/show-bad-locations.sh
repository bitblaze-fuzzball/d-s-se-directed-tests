ctxbefore=2
ctxafter=5
for l in $(sh compute-bad-locations.sh | cut -f 2- -d " ") ; do f=$(echo $l | cut -f 1 -d ":"); l=$(echo $l | cut -f 2 -d ":"); echo "$f:$l" ; echo "===================================================" ; head -n $((l+ctxafter)) $f | tail -n $((ctxbefore+ctxafter)); echo ; done

