#!/bin/sh
# gate edges plus the settled pitch of every high segment (ignores ms-rate glide steps)
awk '$2!=g { if (g==1) print "on", st, "off", $1, "cv", cv; st=$1 } {g=$2; cv=$3}' "$1"
