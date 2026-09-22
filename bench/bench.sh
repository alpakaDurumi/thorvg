set -e
REPO=$(cd "$1" && pwd)
MODFILE=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
T=$3
BENCH=$(cd "$(dirname "$4")" && pwd)/$(basename "$4")
W=$HOME/hf
rm -rf $W
mkdir -p $W/base $W/mod
git -c safe.directory='*' -C $REPO archive 2e6706fc | tar -x -C $W/base
git -c safe.directory='*' -C $REPO archive 2e6706fc | tar -x -C $W/mod
cp $MODFILE $W/mod/src/renderer/cpu_engine/tvgSwPostEffect.cpp
diff -rq $W/base $W/mod || true
export CC=clang CXX=clang++
for t in base mod; do
    for v in simd:true: scalar-omp:false:openmp simd-omp:true:openmp; do
        n=${v%%:*}; r=${v#*:}; s=${r%%:*}; x=${r#*:}
        b=$W/build-$t-$n
        meson setup $b $W/$t -Dsimd=$s -Dengines=cpu -Dloaders= -Ddefault_library=static -Dbuildtype=release -Dextra=$x >/dev/null
        ninja -C $b >/dev/null
        echo "$t-$n: $(grep -E 'AVX|NEON|OPENMP' $b/config.h | tr '\n' ' ')"
        clang++ -O2 -std=c++14 -I$W/$t/inc $BENCH $b/src/libthorvg-1.a -fopenmp -lpthread -o $W/bench-$t-$n
    done
done
cd $W
uname -m
cat /proc/loadavg
for v in simd:1 scalar-omp:$T simd-omp:$T; do
    n=${v%%:*}; th=${v#*:}
    hyperfine -N -w 1 -r 10 --export-json $n.json -n base-$n "./bench-base-$n 300 $th" -n mod-$n "./bench-mod-$n 300 $th"
done
for n in simd scalar-omp simd-omp; do
    python3 -c "import json; r = {x['command']: x for x in json.load(open('$n.json'))['results']}; b, m = r['base-$n'], r['mod-$n']; print(f\"RESULT $n: base {b['mean']*1000:.1f} ms, mod {m['mean']*1000:.1f} ms, time {(m['mean']/b['mean']-1)*100:+.1f}%\")"
done
