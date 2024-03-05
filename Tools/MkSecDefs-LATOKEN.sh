#! /bin/bash

Here=`dirname $0`

# Run the SQL query:
Tmp=/tmp/MkSecDefs-LATOKEN.out

psql -h 192.168.10.11 cexprd leonid_merkin \
  -f "$Here"/MkSecDefs-LATOKEN.sql \
  -o "$Tmp"

# Post-processing: AWK:
awk -F'|' -f "$Here"/MkSecDefs-LATOKEN.awk "$Tmp"
rm "$Tmp"
