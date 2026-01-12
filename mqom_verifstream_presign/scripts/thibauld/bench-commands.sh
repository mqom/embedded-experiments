NO_BENCHMARK_TIME=1 EXTRA_CFLAGS="-DBENCHMARK -DBENCHMARK_CYCLES" python3 manage.py compile cat1 cat5
python3 manage.py bench cat1 cat5 -n 1000

NO_BENCHMARK_TIME=1 EXTRA_CFLAGS="-DBENCHMARK_CYCLES" FORCE_PLATFORM_AVX2=1 python3 manage.py compile cat1 cat5

