set -e
sudo apt-get update -qq
sudo apt-get install -y -qq clang meson ninja-build libomp-dev >/dev/null
echo "cpus=$(nproc) $(lscpu | grep -m1 'Model name')"
clang++ --version | head -1
R=$(pwd)
W=$HOME/fb
rm -rf $W
mkdir -p $W/base $W/mod
git archive 2e6706fc | tar -x -C $W/base
git archive 2e6706fc | tar -x -C $W/mod
cp src/renderer/cpu_engine/tvgSwPostEffect.cpp $W/mod/src/renderer/cpu_engine/
for t in base mod; do
    python3 - $W/$t/src/renderer/cpu_engine/tvgSwRenderer.cpp <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
old = """        case SceneEffect::Fill: {
            return effectFill(p, static_cast<const RenderEffectFill*>(effect), direct);
        }"""
new = """        case SceneEffect::Fill: {
            auto t0 = std::chrono::steady_clock::now();
            auto ret = effectFill(p, static_cast<const RenderEffectFill*>(effect), direct);
            g_fillNs[direct] += std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
            ++g_fillCalls[direct];
            return ret;
        }"""
assert s.count(old) == 1
s = s.replace(old, new)
inc = '#include "tvgSwRenderer.h"\n'
s = s.replace(inc, inc + '#include <chrono>\ndouble g_fillNs[2] = {};\nuint64_t g_fillCalls[2] = {};\n')
open(p, 'w').write(s)
PY
done
export CC=clang CXX=clang++
for t in base mod; do
    for v in simd:true: scalar-omp:false:openmp simd-omp:true:openmp; do
        n=${v%%:*}; r=${v#*:}; s=${r%%:*}; x=${r#*:}
        b=$W/build-$t-$n
        meson setup $b $W/$t -Dsimd=$s -Dengines=cpu -Dloaders= -Ddefault_library=static -Dbuildtype=release -Dextra=$x >/dev/null
        ninja -C $b >/dev/null
        echo "$t-$n: $(grep -E 'NEON|OPENMP' $b/config.h | tr '\n' ' ')"
        clang++ -O2 -std=c++14 -DINSTRUMENT -I$W/$t/inc $R/bench/bench.cpp $b/src/libthorvg-1.a -fopenmp -lpthread -o $W/bench-$t-$n
    done
done
cd $W
for t in base mod; do for n in simd scalar-omp simd-omp; do printf "%-15s %s\n" $t-$n "$(./bench-$t-$n 20 2 2>&1 | head -1)"; done; done
T=$(nproc)
for v in simd:1 scalar-omp:$T simd-omp:$T; do
    n=${v%%:*}; th=${v#*:}
    for b in base mod; do ./bench-$b-$n 300 $th >/dev/null 2>&1; done
    for i in $(seq 1 10); do
        for b in base mod; do
            echo "$b $(./bench-$b-$n 300 $th 2>&1 | awk '/us\/call/{print $2 + 2 * $5}')" >> $n.txt
        done
    done
    python3 - $n $th <<'PY'
import sys, statistics as st
n, t = sys.argv[1], sys.argv[2]
rows = [l.split() for l in open(f"{n}.txt")]
v = {b: [float(r[1]) for r in rows if r[0] == b] for b in ("base", "mod")}
mb, mm = st.mean(v["base"]), st.mean(v["mod"])
lb, lm = min(v["base"]), min(v["mod"])
print(f"RESULT {n:10s} threads={t} | mean {mb:.1f} -> {mm:.1f} us ({(1 - mm / mb) * 100:+.1f}% less) | min {lb:.1f} -> {lm:.1f} us ({(1 - lm / lb) * 100:+.1f}% less)")
PY
done
