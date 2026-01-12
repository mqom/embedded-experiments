import os, shutil, re

DESTINATION_PATH = os.path.dirname( __file__ ) + '/release_supercop'
MQOM2_C_SOURCE_CODE_FOLDER = os.path.dirname( __file__ ) + '/../../mqom2_ref'

MQOM2_C_SOURCE_CODE_SUBFOLDERS = ['blc', 'fields', 'piop', 'rijndael']
MQOM2_C_SOURCE_CODE_FILES = [
    'api.h',
    'common.h',
    'benchmark.h',
    'enc.h',
    'expand_mq.c',
    'expand_mq.h',
    'fields.h',
    'ggm_tree.c',
    'ggm_tree.h',
    'keygen.c',
    'keygen.h',
    'mqom2_parameters.h',
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

SHA3_C_SOURCE_CODE_COMMON = [
    'sha3/brg_endian.h',
    'sha3/endian_compat.h',
    'sha3/KeccakHash.h',
    'sha3/KeccakHash.c',
    'sha3/KeccakHashtimes4.c',
    'sha3/KeccakHashtimes4.h',
    'sha3/KeccakSponge.h',
    'sha3/KeccakSponge.c',
    'sha3/KeccakSponge.inc',
    'sha3/KeccakSpongetimes4.c',
    'sha3/KeccakSpongetimes4.h',
    'sha3/KeccakSpongetimes4.inc',
    'sha3/SnP-Relaned.h',
    'sha3/macros.h',
    'sha3/config.h',
    'sha3/align.h',
]
SHA3_C_SOURCE_CODE_IMPLEMENTATIONS = [
    ('ref', [
        'sha3/opt64/KeccakP-1600-SnP.h',
        'sha3/opt64/PlSnP-Fallback.inc',
        'sha3/opt64/KeccakP-1600-times4-on1.c',
        'sha3/opt64/KeccakP-1600-64.macros',
        'sha3/opt64/KeccakP-1600-times4-SnP.h',
        'sha3/opt64/KeccakP-1600-opt64-config.h',
        'sha3/opt64/KeccakP-1600-unrolling.macros',
        'sha3/opt64/KeccakP-1600-opt64.c',
        ]
    ),
    ('plain32', [
        'sha3/plain32/KeccakP-1600-times4-on1.c',
        'sha3/plain32/KeccakP-1600-inplace32BI.c',
        'sha3/plain32/KeccakP-1600-SnP.h',
        'sha3/plain32/KeccakP-1600-times4-SnP.h',
        'sha3/plain32/PlSnP-Fallback.inc',
        ]
    ),
    ('avx2', [
        'sha3/avx2/KeccakP-1600-SnP.h',
        'sha3/avx2/KeccakP-1600-AVX2.S',
        'sha3/avx2/KeccakP-1600-times4-SIMD256.c',
        'sha3/avx2/KeccakP-1600-times4-SnP.h',
        'sha3/avx2/KeccakP-1600-unrolling.macros',
        'sha3/avx2/SIMD256-config.h',
        ]
    ),
    ('avx512', [
        'sha3/avx512/KeccakP-1600-SnP.h',
        'sha3/avx512/KeccakP-1600-times4-SIMD512.c',
        'sha3/avx512/SIMD512-4-config.h',
        'sha3/avx512/KeccakP-1600-times4-SnP.h',
        'sha3/avx512/KeccakP-1600-AVX512.S',
        ]
    ),
]

def copy_folder(src_path, dst_path, only_root=False):
    for root, dirs, files in os.walk(src_path):
        subpath = root[len(src_path)+1:]
        root_created = False
        for filename in files:
            _, file_extension = os.path.splitext(filename)
            if file_extension in ['.h', '.c' ]:
                if not root_created:
                    os.makedirs(os.path.join(dst_path, subpath))
                    root_created = True
                shutil.copyfile(
                    os.path.join(src_path, subpath, filename),
                    os.path.join(dst_path, subpath, filename)
                )


shutil.rmtree(DESTINATION_PATH, ignore_errors=True)
os.makedirs(DESTINATION_PATH)

TARGET_TMPL = "mqom2cat{}{}{}{}"
LEVELS = [1, 3, 5]
FIELDS = [4, 1, 8]
TRADE_OFFS = ["fast", "short"]
VARIANTS = ["r5", "r3"]

for sha3_imp, sha3_files in SHA3_C_SOURCE_CODE_IMPLEMENTATIONS:
    for l in LEVELS:
        for field in FIELDS:
            for trade_off in TRADE_OFFS:
                for variant in VARIANTS:
                    for mem_opt in ['default', 'memopt']:
                        instance_path = os.path.join(
                            DESTINATION_PATH, 'crypto_sign',
                            TARGET_TMPL.format(l, f'gf{2**field}', trade_off, variant), sha3_imp + '_' + mem_opt
                        )
                        shutil.rmtree(instance_path, ignore_errors=True)
                        os.makedirs(instance_path)
                        # Deal with SHA-3 specific files
                        for f in sha3_files:
                            if l == 1 and field == 4 and trade_off == "fast" and variant == "r5" and mem_opt == 'default':
                                shutil.copyfile(
                                    os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, f),
                                    os.path.join(instance_path, os.path.split(f)[1])
                                )
                            else:
                                # Create symlinks
                                base_path = os.path.join(
                                    '../../',
                                    TARGET_TMPL.format(1, f'gf16', "fast", "r5"), sha3_imp+'_default'
                                )
                                os.symlink(os.path.join(base_path, os.path.split(f)[1]), os.path.join(instance_path, os.path.split(f)[1]))

                        # Deal with all the other files
                        for filename in MQOM2_C_SOURCE_CODE_FILES + SHA3_C_SOURCE_CODE_COMMON:
                            if l == 1 and field == 4 and trade_off == "fast" and variant == "r5" and sha3_imp == 'ref' and mem_opt == 'default':
                                shutil.copyfile(
                                    os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, filename),
                                    os.path.join(instance_path, os.path.split(filename)[1])
                                )
                            else:
                                # Create symlinks for common files
                                base_path = os.path.join(
                                    '../../',
                                    TARGET_TMPL.format(1, f'gf16', "fast", "r5"), 'ref'+'_default'
                                )
                                os.symlink(os.path.join(base_path, os.path.split(filename)[1]), os.path.join(instance_path, os.path.split(filename)[1]))
        
                        shutil.copyfile(
                            os.path.join(MQOM2_C_SOURCE_CODE_FOLDER, 'parameters', f'mqom2_parameters_cat{l}-gf{2**field}-{trade_off}-{variant}.h'),
                            os.path.join(instance_path, f'mqom2_parameters_cat{l}-gf{2**field}-{trade_off}-{variant}.h')
                        )
        
                        # Generate "parameters.h" with the proper parameters
                        parameters = "#ifndef __PARAMETERS_H__\n#define __PARAMETERS_H__\n\n"
                        if l == 1:
                            parameters += "#define MQOM2_PARAM_SECURITY 128\n"
                        elif l == 3:
                            parameters += "#define MQOM2_PARAM_SECURITY 192\n"
                        else:                    
                            parameters += "#define MQOM2_PARAM_SECURITY 256\n"
                        #
                        parameters += ("#define MQOM2_PARAM_BASE_FIELD %d\n" % field)
                        #
                        if trade_off == "short":
                            parameters += "#define MQOM2_PARAM_TRADEOFF 1\n"
                        else:
                            parameters += "#define MQOM2_PARAM_TRADEOFF 0\n"
                        #
                        if variant == "r3":
                            parameters += "#define MQOM2_PARAM_NBROUNDS 3\n\n"
                        else:
                            parameters += "#define MQOM2_PARAM_NBROUNDS 5\n\n"
                        #
                        if "avx512" in sha3_imp:
                            parameters += "#define FIELDS_AVX512\n\n"
                        elif "avx2" in sha3_imp:
                            parameters += "#define FIELDS_AVX2\n\n"
                        else:
                            parameters += "#define FIELDS_REF\n\n"
                        if mem_opt == 'default':
                            # "Default" profile
                            parameters += "#define NO_MISALIGNED_ACCESSES\n#define USE_PRG_CACHE\n#define USE_PIOP_CACHE\n#define USE_XOF_X4\n#define USE_ENC_X8\n\n#define MQOM2_FOR_SUPERCOP\n\n#endif /* __PARAMETERS_H__ */\n"
                        else:
                            # "Memopt" profile
                            parameters += "#define NO_MISALIGNED_ACCESSES\n#define MEMORY_EFFICIENT_PIOP\n#define MEMORY_EFFICIENT_BLC\n#define MEMORY_EFFICIENT_KEYGEN\n#define USE_XOF_X4\n#define USE_ENC_X8\n\n#define MQOM2_FOR_SUPERCOP\n\n#endif /* __PARAMETERS_H__ */\n"

                        with open(os.path.join(instance_path, 'parameters.h'), 'w') as f:
                            f.write(parameters)
        
                        if l == 1 and field == 4 and trade_off == "fast" and variant == "r5" and sha3_imp == 'ref' and mem_opt == 'default':
                            # Patch the generic parameters to include "parameters.h"
                            with open(os.path.join(instance_path, f'mqom2_parameters.h'), 'r') as f:
                                content = f.read()
                            content = content.replace("#define __MQOM2_PARAMETERS_GENERIC_H__\n", "#define __MQOM2_PARAMETERS_GENERIC_H__\n\n#include \"parameters.h\"\n")
                            content = content.replace("\"parameters/mqom2", "\"mqom2")
                            with open(os.path.join(instance_path, f'mqom2_parameters.h'), 'w') as f:
                                f.write(content)
                            # Patch the "config.h" file from SHA-3 to include the parameters.h
                            with open(os.path.join(instance_path, f'config.h'), 'r') as f:
                                content = f.read()
                            content = content.replace("#define XKCP_has_KeccakP1600times4\n", "#define XKCP_has_KeccakP1600times4\n#include \"parameters.h\"\n")
                            with open(os.path.join(instance_path, f'config.h'), 'w') as f:
                                f.write(content)
    
                        # Generate the "description" file
                        with open(os.path.join(instance_path, f'description'), 'w') as f:
                            def print_field(field):
                                if field == 1:
                                    return "GF(2)"
                                if field == 4:
                                    return "GF(16)"
                                if field == 8:
                                    return "GF(256)"
                            f.write("MQOM v2.1, L%d %s %s %s (memory profile: %s, with %s optimizations)\n" % (l, print_field(field), trade_off, variant, mem_opt, sha3_imp))
    
                        # Generate the "implementors" file
                        if l == 1 and field == 4 and trade_off == "fast" and variant == "r5" and sha3_imp == 'ref' and mem_opt == 'default':
                            with open(os.path.join(instance_path, f'implementors'), 'w') as f:
                                f.write("Ryad Benadjila (CryptoExperts)\nThibauld Feneuil (CryptoExperts)\n")
                        else:
                            # Create symlink
                            base_path = os.path.join(
                                '../../',
                                TARGET_TMPL.format(1, f'gf16', "fast", "r5"), 'ref'+'_default'
                            )
                            os.symlink(os.path.join(base_path, 'implementors'), os.path.join(instance_path, 'implementors'))

                        # Generate the "goal-constbranch" and "goal-constindex" files
                        if l == 1 and field == 4 and trade_off == "fast" and variant == "r5" and sha3_imp == 'ref' and mem_opt == 'default':
                            with open(os.path.join(instance_path, f'goal-constbranch'), 'w') as f:
                                f.write("Reviewed 2025.09.28 by Ryad Benadjila and Thibauld Feneuil\n")
                            with open(os.path.join(instance_path, f'goal-constindex'), 'w') as f:
                                f.write("Reviewed 2025.09.28 by Ryad Benadjila and Thibauld Feneuil\n")
                        else:
                            # Create symlink
                            base_path = os.path.join(
                                '../../',
                                TARGET_TMPL.format(1, f'gf16', "fast", "r5"), 'ref'+'_default'
                            )
                            os.symlink(os.path.join(base_path, 'goal-constbranch'), os.path.join(instance_path, 'goal-constbranch'))
                            os.symlink(os.path.join(base_path, 'goal-constindex'), os.path.join(instance_path, 'goal-constindex'))
   
                        # Generate the "designers" file
                        if l == 1 and field == 4 and trade_off == "fast" and variant == "r5" and sha3_imp == 'ref' and mem_opt == 'default':
                            level_up_instance_path = os.path.dirname(instance_path)
                            with open(os.path.join(level_up_instance_path, f'designers'), 'w') as f:
                                f.write("Ryad Benadjila (CryptoExperts)\nCharles Bouillaguet (Sorbonne University)\nThibauld Feneuil (CryptoExperts)\nMatthieu Rivain (CryptoExperts)\n")
                        else:
                            # Create symlink
                            base_path = os.path.join(
                                '../',
                                TARGET_TMPL.format(1, f'gf16', "fast", "r5"),
                            )                        
                            level_up_instance_path = os.path.dirname(instance_path)
                            if not (os.path.isfile(os.path.join(level_up_instance_path, 'designers')) or os.path.islink(os.path.join(level_up_instance_path, 'designers'))):
                                os.symlink(os.path.join(base_path, 'designers'), os.path.join(level_up_instance_path, 'designers'))
    
                        # Generate a basic Makefile for tests
                        test_makefile = "C_SRCS=$(wildcard *.c)\nASM_SRCS=$(wildcard *.S)\nASM_SRCS+=$(wildcard *.s)\n\nOBJS=$(C_SRCS:.c=.o)\nOBJS+=$(ASM_SRCS:.S=.o)\nOBJS+=$(ASM_SRCS:.s=.o)\nCFLAGS = -Wall -Wextra -Wshadow -I. $(EXTRA_CFLAGS)\n\nall: $(OBJS)\n\nclean:\n\t@rm -rf *.o"
                        if l == 1 and field == 4 and trade_off == "fast" and variant == "r5" and sha3_imp == 'ref' and mem_opt == 'default':
                            with open(os.path.join(instance_path, f'Makefile.test'), 'w') as f:
                                f.write(test_makefile)
                        else:
                            # Create symlink
                            base_path = os.path.join(
                                '../../',
                                TARGET_TMPL.format(1, f'gf16', "fast", "r5"), 'ref'+'_default'
                            )
                            os.symlink(os.path.join(base_path, 'Makefile.test'), os.path.join(instance_path, 'Makefile.test'))
    
                        # Specifically for avx2 and avx512, only target x86 and amd64
                        if 'avx' in sha3_imp:
                            with open(os.path.join(instance_path, f'architectures'), 'w') as f:
                                f.write("x86\namd64\n")



# Now we must replace some stuff: all the #include with a subfolder
include_regex = re.compile(r'#include\s*[<"]([^">]+/[^\s">]+)[">]')

def process_file_for_header(filepath):
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()
    def replacement(match):
        full_path = match.group(1)  # e.g., sha3/aa.h or foo/bar/baz.h
        filename = os.path.basename(full_path)  # e.g., aa.h or baz.h
        return f'#include "{filename}"'
    new_content = include_regex.sub(replacement, content)
    if new_content != content:
        print(f"Modified header for: {filepath}")
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(new_content)

def replace_all_headers(directory):
    for root, dirs, files in os.walk(directory):
        for filename in files:
            if filename.endswith((".c", ".h")):
                path = os.path.join(root, filename)
                if not os.path.islink(path):  # skip symlinks
                    process_file_for_header(path)

replace_all_headers(DESTINATION_PATH)
