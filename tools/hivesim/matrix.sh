#!/bin/sh
# balance matrix: matrix.sh DAYS  -> per difficulty, over 12 world kinds x 3 seeds: average pop, raids won/lost, how many colonies ended with pop <= 4
D=${1:-80}
H="$(dirname "$0")"
gcc -std=gnu99 -O1 -w -I"$H/../../tests/stub" -I"$H/../../src" -o /tmp/hsim "$H/hsim.c" || exit 1
for d in 0 1 2; do
  for k in 0 1 2 3 4 5 6 7 8 9 10 11; do for s in 1 2 3; do /tmp/hsim $D $d $k $s | head -1; done; done |
  awk -v d=$d '{n++; for(i=1;i<=NF;i++){ if($i=="pop") p+=$(i+1); if($i=="raids"){ split($(i+1),a,"/"); w+=a[1]; l+=a[2] } if($i=="score") s+=$(i+1); if($i=="hap") h+=$(i+1)} if($0 ~ /pop [0-4] /) low++ } END{ printf("diff %d: runs %d avgpop %.1f avghap %.0f raids won %.1f lost %.1f per run, score %.0f, collapsed %d\n", d, n, p/n, h/n, w/n, l/n, s/n, low) }'
done
