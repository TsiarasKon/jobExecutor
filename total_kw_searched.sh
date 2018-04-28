#!/bin/bash

res=$(grep -ohr 'search : .* :' ./log | cut -d':' -f2 | sort | uniq | wc -l)
echo "Total number of keywords searched:" "$res"


# Find all search lines with grep
# | get only the matching keyword with cut
# | sort the keywords
# | keep one of each
# | get the number of results with wc -l


# For the record, using this script with 'search : .* : ' (with a space in the end) 
# in the grep above would return the "Total number of keywords found" instead of just "searched"
