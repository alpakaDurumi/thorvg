set -e
cd bench
for v in repo neon2 neonC; do
    clang++ -O3 -std=c++14 -DTHORVG_NEON_SUPPORT "-DFILLROW=\"fillrow_$v.inc\"" -I. fill_check.cpp -o check-$v
    echo "VERIFY $v: $(./check-$v verify-only)"
done
clang++ -O3 -std=c++14 kbench.cpp -o k-old
for v in repo neon2 neonC; do
    clang++ -O3 -std=c++14 -DTHORVG_NEON_SUPPORT "-DFILLROW=\"fillrow_$v.inc\"" -I. kbench.cpp -o k-$v
done
for m in 0 1; do
    hyperfine -N -w 1 -r 10 --export-json k$m.json -n old "./k-old $m 300" -n repo "./k-repo $m 300" -n neon2 "./k-neon2 $m 300" -n neonC "./k-neonC $m 300"
done
for m in 0 1; do
    python3 -c "import json; r = {x['command']: x['mean'] for x in json.load(open('k$m.json'))['results']}; print('RESULT', 'direct' if $m else 'non-direct', ' '.join(f'{k} {v*1000:.1f}ms ({(v/r[\"old\"]-1)*100:+.1f}%)' for k, v in r.items()))"
done
