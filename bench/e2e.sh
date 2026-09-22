set -e
W=$HOME/hf
for b in base-scalar-omp mod-scalar-omp mod-simd mod-simd-omp; do
    clang++ -O2 -std=c++14 -I$W/mod/inc bench/fillcheck.cpp $W/build-$b/src/libthorvg-1.a -fopenmp -lpthread -o $W/fc-$b
    $W/fc-$b > $W/fc-$b.txt
done
cd $W
for b in mod-scalar-omp mod-simd mod-simd-omp; do
    cmp -s fc-base-scalar-omp.txt fc-$b.txt && echo "E2E $b: same as base scalar ($(wc -l < fc-$b.txt) cases)" || echo "E2E $b: DIFF"
done
