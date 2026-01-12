import os, shutil

DESTINATION_PATH = os.path.dirname( __file__ ) + '/release_github'
MQOM2_C_SOURCE_CODE_FOLDER = os.path.dirname( __file__ ) + '/../../mqom2_ref'

MQOM2_C_SOURCE_CODE_SUBFOLDERS = ['blc', 'fields', 'piop', 'rijndael', 'scripts']
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
    'manage.py',
    'Makefile',
    'mqom2_parameters.h',
    'prg.h',
    'prg.c',
    'prg_cache.h',
    'README.md',
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
    'fields/fields_handling.h',
    'fields/fields_avx2.h',
    'fields/fields_avx512.h',
    'fields/fields_common.h',
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
    'scripts/build-release-mupq.py',
]

def copy_folder(src_path, dst_path, only_root=False):
    for root, dirs, files in os.walk(src_path):
        subpath = root[len(src_path)+1:]
        root_created = False
        for filename in files:
            _, file_extension = os.path.splitext(filename)
            if file_extension in ['.h', '.c', '.inc', '.macros', '.S'] or filename in ['Makefile', '.gitignore']:
                if not root_created:
                    os.makedirs(os.path.join(dst_path, subpath))
                    root_created = True
                shutil.copyfile(
                    os.path.join(src_path, subpath, filename),
                    os.path.join(dst_path, subpath, filename)
                )

shutil.rmtree(DESTINATION_PATH, ignore_errors=True)
os.makedirs(DESTINATION_PATH)

for foldername in MQOM2_C_SOURCE_CODE_SUBFOLDERS:
    if foldername == 'scripts':
        foldername = 'export_scripts'
    os.makedirs(os.path.join(DESTINATION_PATH, foldername))

for filename in MQOM2_C_SOURCE_CODE_FILES:
    if filename == 'scripts/build-release-mupq.py':
        shutil.copyfile(
            os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, filename),
            os.path.join(DESTINATION_PATH, filename.replace('scripts', 'export_scripts'))
        )
    else:
        shutil.copyfile(
            os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, filename),
            os.path.join(DESTINATION_PATH, filename)
        )

copy_folder(
    os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, 'sha3'),
    os.path.join(DESTINATION_PATH, 'sha3'),
)
copy_folder(
    os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, 'parameters'),
    os.path.join(DESTINATION_PATH, 'parameters'),
)
copy_folder(
    os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, 'generator'),
    os.path.join(DESTINATION_PATH, 'generator'),
)
copy_folder(
    os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, 'benchmark'),
    os.path.join(DESTINATION_PATH, 'benchmark'),
)

Makefile_lines = None
with open(os.path.join(DESTINATION_PATH, 'Makefile'), 'r') as _file:
    Makefile_lines = _file.readlines()

with open(os.path.join(DESTINATION_PATH, 'Makefile'), 'w') as _file:
    ignore_lines = 0
    for line in Makefile_lines:
        if ignore_lines > 0:
            ignore_lines -= 1
        else:
            data = line.split(':')
            if data[0] in ['test_ggm', 'test_keygen', 'test_blc', 'test_piop', 'test_sign', 'test_embedded_KAT', 'test_matmul', 'mupq_kat_gen']:
                ignore_lines = 2
                continue
            if 'rm' in line and 'test_ggm' in line:
                continue
            if 'CFLAGS ?=' in line:
                data = line[:-1].split(' ')
                data.remove('-g')
                data.append('-DNDEBUG')
                line = ' '.join(data) + '\n'
            _file.write(line)

with open(os.path.join(DESTINATION_PATH, '.gitignore'), 'w') as _file:
    _file.write('build/\n')
    _file.write('*.o')
