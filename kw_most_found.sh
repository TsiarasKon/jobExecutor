#!/bin/bash

echo "Keyword(s) most frequently found:"
lines=$(grep -ohr 'search : .* : ' ./log | cut -d':' -f2- | sort | uniq)
outp=""
while read l; do
	word=$(cut -d' ' -f1 <<< $l)
	wordcount=$(cut -d':' -f2- <<< $l | sed 's/[^:]//g' | wc -c)
	outp+="$wordcount $word
"
done <<<"$lines"
reslines=$(awk '{i[$2]+=$1} END {for(x in i){print i[x]" "x}}' <<< "${outp%?}" | sort -rn)
res=$(awk '$1==a' a=${reslines:0:1} <<< "$reslines" | awk '{print $2" ""[totalNumFilesFound: "$1"]"}')
echo "$res"


# Find all search lines that yielded at least one result with grep returning only 
# the matching pattern, that being "keyword : path1 : path2 : ... : pathN".
# For each of these lines, create a new one in the form of "numOfPaths keyword".
# Using awk, add the numOfPaths for identical keywords 
# | sort the resulting lines for highest to lowest numOfPaths
# Print all the lines with numOfPaths equal to the highest
