#!/bin/bash
OUTP=$(grep -ohr 'search : .* : .*' ./log | cut -d':' -f2- | sort | uniq)
for l in $OUTP; do
	echo $l
	echo $(awk -F':' '{print NF; exit} $l')
done

# find all search lines that yielded at least one result with grep returning only the matching pattern
# | get the matching keyword with cut
# | sort the keywords
# | keep one of each
# | get the number of results with wc -l
