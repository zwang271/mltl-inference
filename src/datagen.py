from utils import *
import numpy as np
import argparse
import random
import time
sys.path.append(os.path.dirname(os.path.realpath(__file__)) + '/libmltl/lib')
import libmltl as mltl

def random_sampling(formula: str, 
                    samples: int,
                    m_delta: int,
                    ) -> tuple[list, list]:
    """
    Randomly samples traces for the formula
    Returns a tuple of positive and negative samples (pos, neg)
    """
    pos, neg = [], []
    target_num = samples // 2
    batch_size = samples
    n, m = get_n(formula), comp_len(formula)
    while len(pos) < target_num or len(neg) < target_num:
        print(f"pos: {len(pos)}, neg: {len(neg)}")
        traces = np.random.randint(0, 2, (batch_size, m+np.random.randint(0, m_delta+1), n))
        traces = [trace.tolist() for trace in traces]
        for i, trace in enumerate(traces):
            traces[i] = [str(row).replace("[", "").replace("]", "").replace("," , "").replace(" ", "") for row in trace]
        results = interpret_batch(formula, traces)
        print(results)
        pos_idx, neg_idx = [], []
        for i, value in results.items():
            if value:
                pos_idx.append(i)
            else:
                neg_idx.append(i)
        batch_pos = [traces[i] for i in pos_idx]
        batch_neg = [traces[i] for i in neg_idx]
        if len(pos) < target_num:
            pos.extend(batch_pos)
            pos = pos[:min(len(pos), target_num)]
        if len(neg) < target_num:
            neg.extend(batch_neg)
            neg = neg[:min(len(neg), target_num)]
    return pos[:target_num], neg[:target_num]

def west_sampling(formula: str, 
                  samples: int,
                  m_delta: int,
                  ) -> tuple[list, list]:
    """
    Samples traces for the formula using the West algorithm
    Returns a tuple of positive and negative samples (pos, neg)
    """
    pos, neg = set(), set()
    target_num = samples // 2
    regex = west(formula)
    while len(pos) < target_num:
        pos.add(random_trace(regex, m_delta))
    regex = west("!" + formula)
    while len(neg) < target_num:
        neg.add(random_trace(regex, m_delta))
    pos, neg = list(pos), list(neg)
    pos = [trace.split(",") for trace in pos]
    neg = [trace.split(",") for trace in neg]
    return pos, neg

def generate_traces(formula: str,
            samples: int,
            m_delta: int,
            max_attempts: int=1e6,
            ) -> tuple[list, list]: 
    '''
    Samples traces for the formula using random sampling via libmltl
    Returns a tuple of positive and negative samples (pos, neg)
    If not enough samples are found within max_attempts traces, return None
    '''   
    ast = mltl.parse(formula)  
    pos, neg = [], []
    target_num = samples // 2
    n, m = get_n(formula), comp_len(formula)
    attempts = 0
    while attempts < max_attempts:
        attempts += 1
        trace = np.random.randint(0, 2, (m+np.random.randint(0, m_delta+1), n))
        trace = trace.tolist()
        trace = [str(row).replace("[", "").replace("]", "").replace("," , "").replace(" ", "") for row in trace]
        verdict = ast.evaluate(trace)
        if verdict and len(pos) < target_num:
            pos.append(trace)
        elif not verdict and len(neg) < target_num:
            neg.append(trace)
        if len(pos) == target_num and len(neg) == target_num:
            return pos, neg
    return None, None

    
if __name__ == '__main__':
    formula = "G[0,4] (p0 & p3)"
    samples = 256
    m_delta = 8
    pos, neg = generate_traces(formula, samples, m_delta)
    if pos is not None:
        print(f"Number of positive samples: {len(pos)}")
        print(f"Number of negative samples: {len(neg)}")
        print(pos[0])
        print(neg[0])
    else:
        print("Not enough samples found")

    exit()
    parser = argparse.ArgumentParser(description='Creates a dataset')
    # required argument: input folder containing formula.txt file
    parser.add_argument('dataset_folder', type=str, 
                        help='Input folder must contain formula.txt file')
    # optional arguments
    parser.add_argument('--samples', type=int, default=1000, 
                        help='Number of samples. Half will be positive, half negative')
    parser.add_argument('--percent_train', type=float, default=0.8,)
    parser.add_argument('--max', type=int, default=10, 
                        help='Maximum interval bound to scale formula to')
    parser.add_argument('--method', type=str, default='random', choices=['random', 'west'],
                        help='Method to generate the dataset')
    parser.add_argument('--seed', type=int, default=None, 
                        help='Seed for random number generator')
    parser.add_argument('--m_delta', type=int, default=4,
                        help='Variation of generated trace length from complen of formula')

    # parse arguments
    args = parser.parse_args()
    dataset_folder = args.dataset_folder
    samples = args.samples
    percent_train = args.percent_train
    method = args.method
    m_delta = args.m_delta
    seed = args.seed
    MAX = args.max

    # check if input folder exists and contains formula.txt, read formula
    if not os.path.exists(dataset_folder):
        print(f'{dataset_folder} does not exist')
        sys.exit(1)
    if not os.path.exists(os.path.join(dataset_folder, 'formula.txt')):
        print(f'{dataset_folder} does not contain formula.txt')
        sys.exit(1)
    with open(os.path.join(dataset_folder, 'formula.txt'), 'r') as f:
        formula = f.read().strip()
    # set seed for random number generator
    random.seed(seed)
    np.random.seed(seed)

    # rescale formula to max interval bound, rename vars 
    formula = scale_intervals(formula, MAX)
    formula = formula.replace("a", "p")
    n, m = get_n(formula), comp_len(formula)
    print(formula)
    print(f'n: {n}, m: {m}')

    # generate dataset
    start = time.perf_counter()
    if method == 'random':
        pos, neg = random_sampling(formula, samples, m_delta)
    elif method == 'west':
        pos, neg = west_sampling(formula, samples, m_delta)
    end = time.perf_counter()
    print(f"Time to generate dataset: {end - start}")
    assert(all([value for k, value in interpret_batch(formula, pos).items()]))
    assert(all([not value for k, value in interpret_batch(formula, neg).items()]))

    # Split pos and neg into train and test, PERCENT_TRAIN% for train
    pos_train = pos[:int(len(pos)*percent_train)]
    pos_test = pos[int(len(pos)*percent_train):]
    neg_train = neg[:int(len(neg)*percent_train)]
    neg_test = neg[int(len(neg)*percent_train):]
    print(f"Number of positive samples: {len(pos)}")
    print(f"Number of positive train samples: {len(pos_train)}")
    print(f"Number of positive test samples: {len(pos_test)}")
    print(f"Number of negative samples: {len(neg)}")
    print(f"Number of negative train samples: {len(neg_train)}")
    print(f"Number of negative test samples: {len(neg_test)}")

    # write dataset to files
    write_traces_to_dir(pos_train, os.path.join(dataset_folder, 'pos_train'))
    write_traces_to_dir(pos_test, os.path.join(dataset_folder, 'pos_test'))
    write_traces_to_dir(neg_train, os.path.join(dataset_folder, 'neg_train'))
    write_traces_to_dir(neg_test, os.path.join(dataset_folder, 'neg_test'))
    print(f"Dataset written to {dataset_folder}")

    # write pos and neg datasets each to one file a summary
    with open(os.path.join(dataset_folder, 'pos_summary.txt'), 'w') as f:
        [f.write(",".join(trace) + '\n') for trace in pos]
    with open(os.path.join(dataset_folder, 'neg_summary.txt'), 'w') as f:
        [f.write(",".join(trace) + '\n') for trace in neg]

    # write metadata to file
    with open(os.path.join(dataset_folder, 'metadata.txt'), 'w') as f:
        f.write(f'formula: {formula}\n')
        f.write(f'n: {n}\n')
        f.write(f'm: {m}\n')
        f.write(f'samples: {samples}\n')
        f.write(f'percent_train: {percent_train}\n')
        f.write(f'method: {method}\n')
        f.write(f'm_delta: {m_delta}\n')
        f.write(f'seed: {seed}\n')
        f.write(f'MAX: {MAX}\n')

