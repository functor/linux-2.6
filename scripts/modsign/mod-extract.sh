#!/bin/sh

sections=`objdump -h $1 |
egrep \(text\|data\) |
    while read sid sname stuff
do
  echo $sname
done`

echo $sections

#objcopy -O binary $sections $1 x.raw
objcopy -O binary -j .text $1 x.raw

echo -n >x.raw
for i in $sections
do
  objcopy -O binary -j $i $1 x.raw.part
  cat x.raw.part >>x.raw
done
rm x.raw.part
mv x.raw $2
