#!/bin/bash
mkdir codeSet
sed -i '1i\#include"include/libsy.h"' *.sy
sed -i 's\getInt\getint\g' *.sy
cp *.sy codeSet/*.c
