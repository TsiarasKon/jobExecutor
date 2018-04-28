#!/bin/bash

echo "Keyword(s) most frequently found:"
lines=$(grep -ohr 'search : .* : .*' ./log | cut -d':' -f2- | sort | uniq)
outp=""
while read l; do
	word=$(cut -d' ' -f1 <<< $l)
	wordcount=$(cut -d':' -f2- <<< $l | sed 's/[^:]//g' | wc -c)
	outp+="$wordcount $word
"
done <<<"$lines"
outp="${outp%?}"	# remove '\n' from end
res=$(awk '{i[$2]+=$1} END {for(x in i){print i[x]" "x}}' <<< "$outp" | sort -n)
ress=$(awk '$1==a' a=${res:0:1} <<< "$res" | awk '{print $2" ""[totalNumFilesFound: "$1"]"}')
echo "$ress"



# EXACTLY the same with kw_most_found.sh, except that here sort is not reversed
