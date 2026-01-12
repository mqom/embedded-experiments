import os, shutil

DESTINATION_PATH= os.path.dirname( __file__ ) + '/release_nist'
MQOM2_C_SOURCE_CODE_FOLDER = os.path.dirname( __file__ ) + '/../../mqom2_ref'

MQOM2_C_SOURCE_CODE_SUBFOLDERS = ['blc', 'fields', 'piop', 'rijndael']
MQOM2_C_SOURCE_CODE_FILES = [
    'api.h',
    'benchmark.h',
    'common.h',
    'enc.h',
    'expand_mq.c',
    'expand_mq.h',
    'fields.h',
    'ggm_tree.c',
    'ggm_tree.h',
    'keygen.c',
    'keygen.h',
    'Makefile',
    'prg.h',
    'prg.c',
    'prg_cache.h',
    'sign.c',
    'sign.h',
    'xof.c',
    'xof.h',
    'blc/seed_commit.h',
    'blc/blc_default.c',
    'blc/blc_default.h',
    'blc/blc_memopt.c',
    'blc/blc_memopt.h',
    'blc/blc.h',
    'fields/fields_avx2.h',
    'fields/fields_avx512.h',
    'fields/fields_common.h',
    'fields/fields_handling.h',
    'fields/fields_ref.h',
    'fields/gf256_mult_table.h',
    'piop/piop_cache.h',
    'piop/piop_default.c',
    'piop/piop_default.h',
    'piop/piop_memopt.c',
    'piop/piop_memopt.h',
    'piop/piop.h',
    'rijndael/aes128_fixsliced_arvmv7m.s',
    'rijndael/aes128_table_arvmv7m.s',
    'rijndael/rijndael_aes_ni.c',
    'rijndael/rijndael_aes_ni.h',
    'rijndael/rijndael_common.h',
    'rijndael/rijndael_ct64_enc.h',
    'rijndael/rijndael_ct64.c',
    'rijndael/rijndael_ct64.h',
    'rijndael/rijndael_platform.h',
    'rijndael/rijndael_ref.c',
    'rijndael/rijndael_ref.h',
    'rijndael/rijndael_table.c',
    'rijndael/rijndael_table.h',
    'rijndael/rijndael.h',
]

OPTIMIZATION_FOLDER_NAMES = {
    'ref': 'Reference_Implementation',
    'opt': 'Optimized_Implementation',
}

TARGET_TMPL = "mqom2_cat{}_{}_{}_{}"
LEVELS = [1, 3, 5]
FIELDS = [1, 4, 8]
TRADE_OFFS = ["short", "fast"]
VARIANTS = ["r3", "r5"]

def copy_folder(src_path, dst_path, only_root=False):
    for root, dirs, files in os.walk(src_path):
        subpath = root[len(src_path)+1:]
        root_created = False
        for filename in files:
            _, file_extension = os.path.splitext(filename)
            if file_extension in ['.h', '.c', '.inc', '.macros', '.S'] or filename in ['Makefile', '.gitignore']:
                if not root_created:
                    #print(dst_path, "--", subpath)
                    #print(os.path.join(dst_path, subpath))
                    os.makedirs(os.path.join(dst_path, subpath))
                    root_created = True
                #print(os.path.join(src_path, subpath, filename), '-->', os.path.join(dst_path, subpath, filename))
                shutil.copyfile(
                    os.path.join(src_path, subpath, filename),
                    os.path.join(dst_path, subpath, filename)
                )

for code in OPTIMIZATION_FOLDER_NAMES.keys():
    for l in LEVELS:
        for field in FIELDS:
            for trade_off in TRADE_OFFS:
                for variant in VARIANTS:
                    instance_path = os.path.join(
                        DESTINATION_PATH, OPTIMIZATION_FOLDER_NAMES[code],
                        TARGET_TMPL.format(l, f'gf{2**field}', trade_off, variant)
                    )
                    shutil.rmtree(instance_path, ignore_errors=True)
                    os.makedirs(instance_path)

                    for foldername in MQOM2_C_SOURCE_CODE_SUBFOLDERS:
                        os.makedirs(os.path.join(instance_path, foldername))

                    for filename in MQOM2_C_SOURCE_CODE_FILES:
                        shutil.copyfile(
                            os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, filename),
                            os.path.join(instance_path, filename)
                        )

                    shutil.copyfile(
                        os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, 'parameters', f'mqom2_parameters_cat{l}-gf{2**field}-{trade_off}-{variant}.h'),
                        os.path.join(instance_path, 'mqom2_parameters.h')
                    )

                    copy_folder(
                        os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, 'sha3'),
                        os.path.join(instance_path, 'sha3'),
                    )
                    copy_folder(
                        os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, 'generator'),
                        os.path.join(instance_path, 'generator'),
                    )
                    copy_folder(
                        os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, 'benchmark'),
                        os.path.join(instance_path, 'benchmark'),
                    )

                    Makefile_lines = None
                    with open(os.path.join(instance_path, 'Makefile'), 'r') as _file:
                        Makefile_lines = _file.readlines()

                    with open(os.path.join(instance_path, 'Makefile'), 'w') as _file:
                        # Keccak
                        #if code in ['avx2', 'avx2_gfni', 'avx512_gfni']:
                        #    _file.write(f'KECCAK_PLATFORM=avx2\n')
                        #else:
                        #    _file.write(f'KECCAK_PLATFORM=opt64\n')
                        
                        # Rijndael
                        #if code in ['avx2', 'avx2_gfni', 'avx512_gfni']:
                        #    _file.write(f'RIJNDAEL_AES_NI=1\n')
                        #else:
                        #    _file.write(f'RIJNDAEL_CONSTANT_TIME_REF=1\n')

                        # Fields
                        #if code == 'ref':
                        #    _file.write(f'FIELDS_REF=1\n')
                        #if code == 'avx2':
                        #    _file.write(f'FIELDS_AVX2=1\n')
                        #if code == 'avx2_gfni':
                        #    _file.write(f'FIELDS_AVX2=1\n')
                        #if code == 'avx512_gfni':
                        #    _file.write(f'FIELDS_AVX512=1\n')

                        # Instruction Sets
                        #if code == 'avx2':
                        #    _file.write(f'FORCE_PLATFORM_AVX2=1\n')
                        #if code == 'avx2_gfni':
                        #    _file.write(f'FORCE_PLATFORM_AVX2_GFNI=1\n')
                        #if code == 'avx512_gfni':
                        #    _file.write(f'FORCE_PLATFORM_AVX512_GFNI=1\n')

                        #_file.write(f'\n')

                        ignore_lines = 0
                        for line in Makefile_lines:
                            if ignore_lines > 0:
                                ignore_lines -= 1
                            else:
                                data = line.split(':')
                                if data[0] in ['test_ggm', 'test_keygen', 'test_blc', 'test_piop', 'test_sign', 'test_embedded_KAT']:
                                    ignore_lines = 2
                                    continue
                                if ('$(MQOM2_VARIANT)' in line) and 'ifeq' in line:
                                    ignore_lines = 5
                                    continue
                                if 'MQOM2_VARIANT_CFLAGS' in line:
                                    continue
                                if 'rm' in line and 'test_ggm' in line:
                                    continue
                                if 'CFLAGS ?=' in line:
                                    line = line[:-1] + ' -DNDEBUG' + '\n'
                                _file.write(line)
