import os, shutil, re, binascii, hashlib

DESTINATION_PATH = os.path.dirname( __file__ ) + '/release_liboqs'
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

TARGET_TMPL = "mqom2-cat{}-{}-{}-{}"
LEVELS = [1, 3, 5]
#FIELDS = [1, 4, 8]
# XXX: NOTE: only activate GF(16) for libOQS, as we do not want to
# push too much variants
FIELDS = [4]
TRADE_OFFS = ["fast", "short"]
VARIANTS = ["r5", "r3"]

all_sizes = { 
'mqom2_cat1_gf2_short_r5' : (52, 72, 2820),
'mqom2_cat1_gf2_short_r3' : (52, 72, 2868),
'mqom2_cat1_gf2_fast_r5' : (52, 72, 3144),
'mqom2_cat1_gf2_fast_r3' : (52, 72, 3212),
'mqom2_cat1_gf16_short_r5' : (60, 88, 2916),
'mqom2_cat1_gf16_short_r3' : (60, 88, 3060),
'mqom2_cat1_gf16_fast_r5' : (60, 88, 3280),
'mqom2_cat1_gf16_fast_r3' : (60, 88, 3484),
'mqom2_cat1_gf256_short_r5' : (80, 128, 3156),
'mqom2_cat1_gf256_short_r3' : (80, 128, 3540),
'mqom2_cat1_gf256_fast_r5' : (80, 128, 3620),
'mqom2_cat1_gf256_fast_r3' : (80, 128, 4164),
'mqom2_cat3_gf2_short_r5' : (78, 108, 6280),
'mqom2_cat3_gf2_short_r3' : (78, 108, 6388),
'mqom2_cat3_gf2_fast_r5' : (78, 108, 7414),
'mqom2_cat3_gf2_fast_r3' : (78, 108, 7576),
'mqom2_cat3_gf16_short_r5' : (90, 132, 6496),
'mqom2_cat3_gf16_short_r3' : (90, 132, 6820),
'mqom2_cat3_gf16_fast_r5' : (90, 132, 7738),
'mqom2_cat3_gf16_fast_r3' : (90, 132, 8224),
'mqom2_cat3_gf256_short_r5' : (120, 192, 7036),
'mqom2_cat3_gf256_short_r3' : (120, 192, 7900),
'mqom2_cat3_gf256_fast_r5' : (120, 192, 8548),
'mqom2_cat3_gf256_fast_r3' : (120, 192, 9844),
'mqom2_cat5_gf2_short_r5' : (104, 144, 11564),
'mqom2_cat5_gf2_short_r3' : (104, 144, 11764),
'mqom2_cat5_gf2_fast_r5' : (104, 144, 13124),
'mqom2_cat5_gf2_fast_r3' : (104, 144, 13412),
'mqom2_cat5_gf16_short_r5' : (122, 180, 12014),
'mqom2_cat5_gf16_short_r3' : (122, 180, 12664),
'mqom2_cat5_gf16_fast_r5' : (122, 180, 13772),
'mqom2_cat5_gf16_fast_r3' : (122, 180, 14708),
'mqom2_cat5_gf256_short_r5' : (160, 256, 12964),
'mqom2_cat5_gf256_short_r3' : (160, 256, 14564),
'mqom2_cat5_gf256_fast_r5' : (160, 256, 15140),
'mqom2_cat5_gf256_fast_r3' : (160, 256, 17444),
}

def get_param_security(l):
    if l == 1:
        return 128
    if l == 3:
        return 192
    if l == 5:
        return 256

def get_param_tradeoff(t):
    if t == "fast":
        return 0
    else:
        return 1

def get_param_variant(r):
    if r == "r3":
        return 3
    else:
        return 5

for l in LEVELS:
    for field in FIELDS:
        for trade_off in TRADE_OFFS:
            for variant in VARIANTS:
                name = TARGET_TMPL.format(l, f'gf{2**field}', trade_off, variant).replace("-", "_")
                out_meta_yml = open(DESTINATION_PATH + "/META_%s.yml" % name, 'w')
                out_meta_yml.write("name: %s\n" % name)
                out_meta_yml.write("type: signature\n")
                out_meta_yml.write("principal-submitters:\n")
                out_meta_yml.write(" - Ryad Benadjila\n - Charles Bouillaguet\n - Thibauld Feneuil\n - Matthieu Rivain\n")
                out_meta_yml.write("crypto-assumption: Computing the solution of a Multivariate Quadratic problem.\n")
                out_meta_yml.write("website: https://mqom.org/\n")
                out_meta_yml.write("spec-version: Round 2\n")
                out_meta_yml.write("claimed-nist-level: %d\n" % l)
                out_meta_yml.write("claimed-security: EUF-CMA\n")
                (pub_size, priv_size, sig_size) = all_sizes[name]
                out_meta_yml.write("length-public-key: %d\n" % pub_size)
                out_meta_yml.write("length-secret-key: %d\n" % priv_size)
                out_meta_yml.write("length-signature: %d\n" % sig_size)
                # Hash the KAT response file
                kat_name = "../build_ref/"+name.replace("mqom2_", "") + ("/PQCsignKAT_%d.rsp" % priv_size)
                # Open the file and hash it
                sha256digest = hashlib.sha256()
                with open(kat_name, 'r') as f:
                    content = f.read()
                    sha256digest.update(content.encode('latin-1')) 
                out_meta_yml.write("nistkat-sha256: {0}\n".format(sha256digest.hexdigest()))
                out_meta_yml.write("implementations-switch-on-runtime-cpu-features: false\n")
                out_meta_yml.write("crypto-assumption: Solving a Multivariate Quadratic (MQ) instance.\n")                
                out_meta_yml.write("common_dep:\n")
                out_meta_yml.write("  - aes\n")
                out_meta_yml.write("  - sha3\n")
                out_meta_yml.write("implementations:\n")
                for mem_opt in ['default', 'memopt', 'avx2']:
                    out_meta_yml.write("  - name: %s\n" % mem_opt)
                    out_meta_yml.write("    version: 2.1\n")
                    out_meta_yml.write("    folder_name: .\n")
                    out_meta_yml.write("    signature_keypair: %s_%s_crypto_sign_keypair\n" % (name, mem_opt))
                    out_meta_yml.write("    signature_signature: %s_%s_crypto_sign_signature\n" % (name, mem_opt))
                    out_meta_yml.write("    signature_verify: %s_%s_crypto_sign_verify\n" % (name, mem_opt))
                    out_meta_yml.write("    compile_opts: -DMQOM2_FOR_LIBOQS -DAPPLY_NAMESPACE=%s_%s_ -DMQOM2_PARAM_SECURITY=%d -DMQOM2_PARAM_BASE_FIELD=%d -DMQOM2_PARAM_TRADEOFF=%d -DMQOM2_PARAM_NBROUNDS=%d" % (name, mem_opt, get_param_security(l), field, get_param_tradeoff(trade_off), get_param_variant(variant)))
                    if mem_opt == "default":
                        out_meta_yml.write(" -DUSE_PRG_CACHE -DUSE_PIOP_CACHE -DUSE_XOF_X4 -DUSE_ENC_X8\n")
                    elif mem_opt == "avx2":
                        out_meta_yml.write(" -DUSE_PRG_CACHE -DUSE_PIOP_CACHE -DUSE_XOF_X4 -DUSE_ENC_X8 -DFIELDS_AVX2\n")
                    else:
                        out_meta_yml.write(" -DMEMORY_EFFICIENT_BLC -DMEMORY_EFFICIENT_PIOP -DMEMORY_EFFICIENT_KEYGEN -DUSE_XOF_X4\n")
                    out_meta_yml.write("    sources:")
                    for f in MQOM2_C_SOURCE_CODE_FILES:
                        out_meta_yml.write(" "+f)
                    out_meta_yml.write(" parameters/"+name.replace("_", "-").replace("mqom2-", "mqom2_parameters_")+".h")
                    out_meta_yml.write("\n")
                    out_meta_yml.write("    no-secret-dependent-branching-claimed: true\n")
                    out_meta_yml.write("    no-secret-dependent-branching-checked-by-valgrind: true\n")
                    if mem_opt == "default" or mem_opt == "avx2":
                        out_meta_yml.write("    large-stack-usage: true\n")
                    else:
                        out_meta_yml.write("    large-stack-usage: false\n")
                    if mem_opt == "avx2":
                        out_meta_yml.write("    supported_platforms:\n")
                        out_meta_yml.write("      - architecture: x86_64\n")
                        out_meta_yml.write("        required_flags:\n")
                        out_meta_yml.write("          - avx2\n")
                        out_meta_yml.write("          - aes\n")
                    else:
                        out_meta_yml.write("    supported-platforms: all\n")

                out_meta_yml.close()
