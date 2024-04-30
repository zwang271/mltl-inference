import json
import os
import pathlib
import sys
from learn_ltl_matrix import train
from matrix2ltl import test_matrix

# python3 learn_ltl_matrix.py -train_file=dataset.json -test_file=dataset.json -save_model=test.model -log_file=test.log -res_file=test.txt -epoch_num=20
# python3 matrix2ltl.py -train_file=dataset.json -test_file=dataset.json -save_model=test.model -top_num=100

def load_traces(dir: str) -> list[list[str]]:
    '''
    Input
        dir: the directory to load traces from
    Output
        traces: a list of traces
    '''
    traces = []
    for filename in os.listdir(dir):
        with open(os.path.join(dir, filename), 'r') as f:
            trace = [line.strip().replace(",","") for line in f]
            traces.append(trace)
    return traces

def bit2set(trace_bits: list[list[str]]) -> list[list[str]]:
    '''
    Input
        trace_bits: a list of traces, each trace is a list of bits
    Output
        trace_sets: a list of traces, each trace is a list of set of bits
    '''
    n = len(trace_bits[0])
    vocab = [f"p{i}" for i in range(n)]
    trace_sets = []
    for state in trace_bits:
        state_set = []
        for i, bit in enumerate(state):
            if bit == "1":
                state_set.append(vocab[i])
        trace_sets.append(state_set)
    return trace_sets
    
def write_json(file, pos, neg, vocab, formula):
    '''
    Input
        file: the file to write to
        pos: a list of positive traces
        neg: a list of negative traces
        vocab: a list of propositions
    '''
    D = {
        "traces_pos": pos,
        "traces_neg": neg,
        "vocab": vocab,
        "ltlf_str": formula,
        "ltlftree": formula
    }
    with open(file, "w") as f:
        json.dump(D, f)

def main(dataset):
    dataset = pathlib.Path(dataset)
    POS_TRAIN = load_traces(dataset / "pos_train")
    NEG_TRAIN = load_traces(dataset / "neg_train")
    POS_TEST = load_traces(dataset / "pos_test")
    NEG_TEST = load_traces(dataset / "neg_test")
    with open (dataset / "metadata.txt", "r") as f:
        formula = f.readline().strip().replace("formula: ", "")

    n = len(POS_TRAIN[0][0])
    vocab = [f"p{i}" for i in range(n)]

    POS_TRAIN = [bit2set(trace) for trace in POS_TRAIN]
    NEG_TRAIN = [bit2set(trace) for trace in NEG_TRAIN]
    POS_TEST = [bit2set(trace) for trace in POS_TEST]
    NEG_TEST = [bit2set(trace) for trace in NEG_TEST]

    write_json(dataset / "train.json", POS_TRAIN, NEG_TRAIN, vocab, formula)
    write_json(dataset / "test.json", POS_TEST, NEG_TEST, vocab, formula)
    print(f"Wrote to {dataset / 'train.json'} and {dataset / 'test.json'}")
    
if __name__ == '__main__':
    dataset = sys.argv[1]
    if not os.path.exists(dataset):
        print(f"Dataset folder {dataset} does not exist.")
        sys.exit(1)
    main(dataset)

    dataset = pathlib.Path(dataset)
    logs = pathlib.Path("logs")
    train_file = dataset / "train.json"
    test_file = dataset / "test.json"
    save_model = (logs / dataset.name).with_suffix(".model")
    log_file = (logs / dataset.name).with_suffix(".log")
    res_file = (logs / dataset.name).with_suffix(".txt")
    network_len = 20
    epoch_num = 50
    lr = 1e-4
    l1 = 0.1

    train(train_file=train_file, 
          formula_len=network_len, 
          save_name=save_model, 
          epoch_num=epoch_num, 
          log_file=log_file,
          learn_rate=lr,
          l1_cof=l1)
    
    res = test_matrix(
        model_name=save_model,
        train_file_name=train_file,
        test_file_name=test_file,
        top_num=100
    )
    acc, pre, rec, int_time, refine_time = res[0],res[1],res[2],res[3],res[4]
    target_ltl = res[-2]
    all_learned = res[-1]
    best_learned = all_learned[0]
    # best_learned = res[-1]
    print('acc:%f, pre:%f, rec:%f, interpretation time:%f'%(acc,pre,rec,int_time+refine_time))
    print('target ltl:', target_ltl)
    print('best learned ltl:', best_learned)

    with open(res_file, "w") as f:
        f.write(f"best formula: {str(best_learned)}\n")
        f.write(f"target formula: {target_ltl}\n")
        f.write(f"accuracy: {acc}\n")
        f.write(f"precision: {pre}\n")
        f.write(f"recall: {rec}\n")
        f.write(f"all learned formulas: \n")
        for formula in all_learned:
            f.write(f"{str(formula)}\n")


