#!/bin/bash

compilers=(gcc clang)
platform_conf=("FORCE_PLATFORM_AVX512_GFNI=1" "FORCE_PLATFORM_AVX512=1" "FORCE_PLATFORM_AVX2_GFNI=1" "FORCE_PLATFORM_AVX2=1" "FORCE_PLATFORM_REF=1")
mem_opt_conf=("" "USE_PIOP_CACHE=0 MEMORY_EFFICIENT_PIOP=1 MEMORY_EFFICIENT_BLC=1 MEMORY_EFFICIENT_KEYGEN=1")
extra_cflags=("" "-O1" "-O2" "-Os" "-O")

for c in ${compilers[@]}
do
	for extra in ${extra_cflags[@]}
	do
		echo "================ Testing $c $extra FORCE_PLATFORM_AVX512_GFNI=1 DEFAULT"
		rm -rf build
		EXTRA_CFLAGS="$extra" CC="$c" FORCE_PLATFORM_AVX512_GFNI=1 python3 manage.py compile -p -1 all
		python3 manage.py test -c build_ref/ -p -1 all
		#
		echo "================ Testing $c $extra FORCE_PLATFORM_AVX512=1 DEFAULT"
		rm -rf build
		EXTRA_CFLAGS="$extra" CC="$c" FORCE_PLATFORM_AVX512=1 python3 manage.py compile -p -1 all
		python3 manage.py test -c build_ref/ -p -1 all
		#
		echo "================ Testing $c $extra FORCE_PLATFORM_AVX2_GFNI=1 DEFAULT"
		rm -rf build
		EXTRA_CFLAGS="$extra" CC="$c" FORCE_PLATFORM_AVX2_GFNI=1 python3 manage.py compile -p -1 all
		python3 manage.py test -c build_ref/ -p -1 all
		#
		echo "================ Testing $c $extra FORCE_PLATFORM_AVX2=1 DEFAULT"
		rm -rf build
		EXTRA_CFLAGS="$extra" CC="$c" FORCE_PLATFORM_AVX2=1 python3 manage.py compile -p -1 all
		python3 manage.py test -c build_ref/ -p -1 all
		###########
		echo "================ Testing $c $extra FORCE_PLATFORM_AVX512_GFNI=1 MEMOPT"
		rm -rf build
		EXTRA_CFLAGS="$extra" CC="$c" FORCE_PLATFORM_AVX512_GFNI=1 USE_PIOP_CACHE=0 MEMORY_EFFICIENT_PIOP=1 MEMORY_EFFICIENT_BLC=1 MEMORY_EFFICIENT_KEYGEN=1 python3 manage.py compile -p -1 all
		python3 manage.py test -c build_ref/ -p -1 all
		#
		echo "================ Testing $c $extra FORCE_PLATFORM_AVX512=1 MEMOPT"
		rm -rf build
		EXTRA_CFLAGS="$extra" CC="$c" FORCE_PLATFORM_AVX512=1 USE_PIOP_CACHE=0 MEMORY_EFFICIENT_PIOP=1 MEMORY_EFFICIENT_BLC=1 MEMORY_EFFICIENT_KEYGEN=1 python3 manage.py compile -p -1 all
		python3 manage.py test -c build_ref/ -p -1 all
		#
		echo "================ Testing $c $extra FORCE_PLATFORM_AVX2_GFNI=1 MEMOPT"
		rm -rf build
		EXTRA_CFLAGS="$extra" CC="$c" FORCE_PLATFORM_AVX2_GFNI=1 USE_PIOP_CACHE=0 MEMORY_EFFICIENT_PIOP=1 MEMORY_EFFICIENT_BLC=1 MEMORY_EFFICIENT_KEYGEN=1 python3 manage.py compile -p -1 all
		python3 manage.py test -c build_ref/ -p -1 all
		#
		echo "================ Testing $c $extra FORCE_PLATFORM_AVX2=1 MEMOPT"
		rm -rf build
		EXTRA_CFLAGS="$extra" CC="$c" FORCE_PLATFORM_AVX2=1 USE_PIOP_CACHE=0 MEMORY_EFFICIENT_PIOP=1 MEMORY_EFFICIENT_BLC=1 MEMORY_EFFICIENT_KEYGEN=1 python3 manage.py compile -p -1 all
		python3 manage.py test -c build_ref/ -p -1 all
	done
done
