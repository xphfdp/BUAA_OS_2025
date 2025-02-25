#!/bin/bash
touch AAA
touch BBB
sed -n '8p' AAA >> BBB
sed -n '32p' AAA >> BBB
sed -n '128p' AAA >> BBB
sed -n '512p' AAA >> BBB
sed -n '1024p' AAA >> BBB
