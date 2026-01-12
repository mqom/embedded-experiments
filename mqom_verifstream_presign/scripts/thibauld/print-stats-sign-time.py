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

