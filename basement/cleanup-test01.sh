#!/bin/bash

# Verify that the program produces identical output in all
# cases.

set -e

for opt in 0 1 2 3 s
do
    gcc -Wall -O$opt cleanup-test01.c -o cleanup-test01
    ./cleanup-test01 | md5sum
done

./cleanup-test01
