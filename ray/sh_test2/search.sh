#!/bin/bash
#First you can use grep (-n) to find the number of lines of string.
#Then you can use awk to separate the answer
grep -n int file >> result
sed -i 's/:int a=1//g' result
