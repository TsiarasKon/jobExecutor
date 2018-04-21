#!/bin/bash
grep -ohr 'search:.*:' ./log | cut -d':' -f2 | sort | uniq | wc -l

# find all search lines with grep
# | get only the matching keyword with cut
# | sort the keywords
# | keep one of each
# | get the number of results with wc -l
