#!/bin/sh
# Run this script and mail to snips-dev@navya.com  for a new OS
#
echo System is `uname -s -r`

uptime

pstat
pstat -T

nfsstat

df
df -k
df -k -i

iostat
iostat -w 5 -c 3

vmstat
vmstat -w 5 -c 3

# No way to do a netstat 5 and then break out after two lines...
netstat

ping

man df
