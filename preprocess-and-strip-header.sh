#!/bin/bash
if (( $# != 1 )); then
  echo "Please provide one header name on stdin"
  exit 1
fi

make -s cdecl

gcc -E "$1" | \
    grep -v include | \
    grep -v __att | \
    grep -ve \([1-9] | \
    grep -v "   ))" | \
    tr -d "\n" | \
    sed -e 's/;/;\n/g' | \
    while IFS= read -r line; \
      do echo "$line"; \
      ./cdecl "$line"; \
      echo ; \
    done
