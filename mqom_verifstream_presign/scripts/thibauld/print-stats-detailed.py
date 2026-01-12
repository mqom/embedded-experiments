import json, sys
all_data = None

# Are we provided a dedicated file to process
filename = 'stats.json' # default file to open
if len(sys.argv) > 1:
    filename = sys.argv[1]

try:
    with open(filename, 'r') as _file:
        all_data = json.loads(_file.read())
except:
    print("Error when opening %s ..." % filename)
    sys.exit(-1)

data_dict = {
    data['name']: data for data in all_data
}

def print_measure(label, value):
    if False:
        print(f'{label}: {value/1000000:.2f}M')
    else:
        print(f'{value/1000000:.2f}')

def print_row(label):
    try:
        data = data_dict[label]
        print(f"===== {data['name']} =====")
        print_measure('ExpandMQ', data['detailed_expand_mq_total'][1])
        print_measure('BLC.Commit', data['detailed_blc_commit_total'][1])
        print_measure(' - Seed Tree', data['detailed_blc_commit_expand_trees'][1])
        print_measure(' - Key Sch.', data['detailed_blc_commit_keysch_commit'][1])
        print_measure(' - Seed Commit', data['detailed_blc_commit_seed_commit'][1])
        print_measure(' - PRG', data['detailed_blc_commit_prg'][1])
        print_measure(' - XOF', data['detailed_blc_commit_xof'][1])
        print_measure(' - Arithm', data['detailed_blc_commit_arithm'][1])
        print_measure(' - XOF Global', data['detailed_blc_commit_global_xof'][1])
        print_measure('PIOP.Compute', data['detailed_piop_compute_total'][1])
        print_measure(' - Expand Gamma', data['detailed_piop_compute_expand_batching_mat'][1])
        print_measure(' - Matrix Mult', data['detailed_piop_compute_matrix_mult_ext'][1])
        print_measure(' - Ext t1', data['detailed_piop_compute_compute_t1'][1])
        print_measure(' - P_zi', data['detailed_piop_compute_compute_p_zi'][1])
        print_measure(' - Compute Batch', data['detailed_piop_compute_batch'][1])
        print_measure(' - Add masks', data['detailed_piop_compute_add_masks'][1])
        print_measure('SampleChallenge', data['detailed_sample_challenge_total'][1])
        print_measure('BLC.Open', data['detailed_blc_open_total'][1])
        print_measure('TOTAL', data['sign_cycles'])
        print()
    except:
        return

for cat in ['L1', 'L3', 'L5']:
    for field in ['gf2', 'gf256']:
        for tradeoff in ['short', 'fast']:
            for variant in ['r3', 'r5']:
                print_row(f'MQOM2-{cat}-{field}-{tradeoff}-{variant}')
        print()

