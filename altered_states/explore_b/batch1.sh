#!/usr/bin/env bash
# one thread, sequential: each core for $T seconds
T=${1:-100}
CD="NORTHCAROLINA,SOUTHCAROLINA,NORTHDAKOTA,SOUTHDAKOTA"
declare -a CORES=(
 "mass:MASSACHUSETTS,MISSISSIPPI,MISSOURI,MINNESOTA,TENNESSEE"
 "new:NEWHAMPSHIRE,NEWJERSEY,NEWMEXICO,NEWYORK"
 "virg:WESTVIRGINIA,VIRGINIA,PENNSYLVANIA"
 "calcar:CALIFORNIA,NORTHCAROLINA,SOUTHCAROLINA"
 "aheavy:ALABAMA,ALASKA,OKLAHOMA,NEBRASKA,INDIANA,LOUISIANA"
 "kansas:ARKANSAS,KANSAS,NEBRASKA,ALASKA,ALABAMA,OKLAHOMA"
 "cd_cal:$CD,CALIFORNIA"
 "cd_mass:$CD,MASSACHUSETTS"
 "cd_rhode:$CD,RHODEISLAND"
 "cd_wash:$CD,WASHINGTON"
 "cd_conn:$CD,CONNECTICUT"
 "cd_penn:$CD,PENNSYLVANIA"
 "cd_wv:$CD,WESTVIRGINIA"
 "cd_newh:$CD,NEWHAMPSHIRE"
 "car_cal_lou:NORTHCAROLINA,SOUTHCAROLINA,CALIFORNIA,LOUISIANA"
 "dak_rhode:NORTHDAKOTA,SOUTHDAKOTA,RHODEISLAND,ILLINOIS"
)
for e in "${CORES[@]}"; do
  name=${e%%:*}; core=${e#*:}
  ./build/core --core "$core" --time "$T" --seed 7 --fill 1000000 --polish 1500000 > ../results/explore_b/core_$name.log
  echo "$name $(grep RESULT ../results/explore_b/core_$name.log | sed 's/.*layouts=/layouts=/') $(grep 'blank hist' ../results/explore_b/core_$name.log)"
done
