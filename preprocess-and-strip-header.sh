#!/bin/bash

# preprocess-and-strip-header.sh slightly reformats header file contents and
# pipes it to cdecl for processing.  As noted in README-cdecl, cdecl is a
# line-oriented processor with no stored state, so the only types it recognizes
# are those from the C standard, which are hard-coded into the program.

# The choice not to exit on error is intentional.
set -u

if (( $# != 1 )); then
  echo "Please provide one header name on stdin."
  exit 1
fi

make -s cdecl

# Exclude indigestible, non-declaration content.
gcc -E "$1" | \
    # exclude "include" directives.
    grep -v include | \
    # exclude compiler attributes.
    grep -v __att | \
    # Remove blank lines.
    tr -d "\n" | \
    sed -e 's/;/;\n/g' | \
    sed -e 's/([1-9])//g' | \
    sed -e 's/__extension__//g' | \
    # collapse multiple spaces
    sed -e 's/[[:space:]]+/ /g' | \
    sed -e 's/))//g' | \
    # Read the content one line at a time by unsetting the Internal Field Separator.
    while IFS= read -r line; \
      do echo Input: "$line"; \
      # Wrap the content in quotes.
      ./cdecl "$line"; \
      echo ; \
    done
