import os
import sys
import tempfile
import subprocess
from pprint import pprint
import random
import re
import mltl_parser

if sys.platform == 'win32':
    INTERPRET_PATH = os.path.join('MLTL_interpreter', 'bin', 'interpret.exe')
    INTERPRET_BATCH_PATH = os.path.join('MLTL_interpreter', 'bin', 'interpret_batch.exe')
    WEST_PATH = './west.exe'
else:
    INTERPRET_PATH = os.path.join('MLTL_interpreter', 'bin', 'interpret')
    INTERPRET_BATCH_PATH = os.path.join('MLTL_interpreter', 'bin', 'interpret_batch')
    WEST_PATH = './west'


def interpret(formula: str, trace: list[str]) -> bool:
    '''
    Input
        formula: a MLTL formula
        trace: a list of strings, each string is the variable values at a time step
    Output
        result: the verdict of the formula on the trace
        i.e. True if the trace satisifes the formula, False otherwise
    '''
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
        f.write(formula)
        formula_path = f.name
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
        for line in trace:
            f.write(",".join(list(line)) + "\n")
        trace_path = f.name
    with tempfile.NamedTemporaryFile(delete=False) as outfile:
        outfile = outfile.name
    subprocess.run(f"{INTERPRET_PATH} {formula_path} {trace_path} {outfile}", 
                stdout=subprocess.DEVNULL, 
                stderr=subprocess.DEVNULL, 
                shell=True)
    with open(outfile, 'r') as f:
        result = f.read()
    return True if result == "0" else False

def interpret_batch(formula: str, traces) -> dict:
    '''
    Input
        formula: a MLTL formula
        traces: a directory containing trace files OR a list of traces
    Output
        results: a dictionary of trace files and their verdicts if directory is given
        OR a dictionary of index and verdict if list of traces is given
    NOTE: USING A DIRECTORY IS PREFERRED FOR EFFICIENCY
    '''
    if type(traces) == str and os.path.isdir(traces):
        traces_dir = traces
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            f.write(formula)
            formula_path = f.name
        with tempfile.NamedTemporaryFile(delete=False) as outfile:
            outfile = outfile.name
        subprocess.run(f"{INTERPRET_BATCH_PATH} {formula_path} {traces_dir} {outfile}", 
                    stdout=subprocess.DEVNULL, 
                    stderr=subprocess.DEVNULL, 
                    shell=True)
        results = {}
        with open(outfile, 'r') as f:
            for line in f:
                trace_file, verdict = line.strip().split(":")
                results[trace_file.strip()] = True if verdict.strip() == "1" else False
        return results
    
    elif type(traces) == list:
        # create a temporary directory of traces and call interpret_batch
        with tempfile.TemporaryDirectory() as traces_dir:
            for i, trace in enumerate(traces):
                # create a file for each trace with the name as the index
                with open(os.path.join(traces_dir, f"{i}.txt"), 'w') as f:
                    for line in trace:
                        f.write(",".join(list(line)) + "\n")
            results = interpret_batch(formula, traces_dir)
            renamed_results = {}
            for file, verdict in results.items():
                index = file.replace(str(traces_dir), "").replace(".txt", "").strip()[1:]
                renamed_results[int(index)] = verdict
            return renamed_results
                        
    else:
        print("Invalid traces argument")
        sys.exit(1)

def west(formula: str) -> list[str]:
    '''
    Input
        formula: a MLTL formula
    Output
        result: the set of all possible traces that satisfy the formula
    '''
    subprocess.run(f"cd WEST/ && {WEST_PATH} \'{formula}\' && cd ..", 
                stdout=subprocess.DEVNULL, 
                stderr=subprocess.DEVNULL, 
                shell=True)
    outfile = os.path.join('WEST', 'output', 'output.txt')
    with open(outfile, 'r') as f:
        result = f.read().splitlines()
    return result[1:]

def random_trace(regexp: list[str], m_delta: int=0, seed=None):
    '''
    Input
        regexp: a list of regular expressions
        seed: optional seed for random number generator
    Output
        result: a random trace that satisfies a randomly chosen regular expression
    '''
    if seed:
        random.seed(seed)
    n = len(regexp[0].split(",")[0])
    extra_time = random.randint(0, m_delta)
    suffix = (","+("s"*n)) * extra_time
    # choose a random regular expression in regexp
    r = random.choice(regexp)
    r = r + suffix
    # randomly replace 's' with '0' or '1'
    for i in range(len(r)):
        if r[i] == 's':
            r = r[:i] + str(random.randint(0, 1)) + r[i+1:]
    return r

def comp_len(formula: str) -> int: 
    '''
    Input
        formula: a MLTL formula
    Output
        result: the computation length of the formula
    Note: computation length = worst-case propagation delay (wpd) = future reach
    '''
    valid, tree, _ = mltl_parser.check_wff(formula)
    if not valid:
        print("Invalid formula")
        sys.exit(1)

    def complen_helper(node):
        if len(node.children) == 1:
            node = node.children[0]
            if node.data == "prop_cons":
                return 0
            elif node.data == "prop_var":
                return 1
            elif node.data == "wff":
                return complen_helper(node)
        
        elif node.children[1].data == "binary_prop_conn":
            return max(complen_helper(node.children[0]), 
                       complen_helper(node.children[2])) 
        
        elif node.children[0].data == "unary_temp_conn":
            interval = str(node.children[1].pretty())
            # find all numbers in interval
            r1 = [int(num) for num in re.findall("\d+", interval)]
            ub = max(r1)
            return ub + complen_helper(node.children[2])
        
        elif node.children[1].data == "binary_temp_conn":
            interval = str(node.children[2].pretty())
            # find all numbers in interval
            r1 = [int(num) for num in re.findall("\d+", interval)]
            ub = max(r1)
            return ub + max(complen_helper(node.children[0]),
                            complen_helper(node.children[3]))
    
    return complen_helper(tree)

def treedepth(formula: str) -> int:
    '''
    Input
        formula: a MLTL formula
    Output
        result: the tree depth of the formula
    '''
    valid, tree, _ = mltl_parser.check_wff(formula)
    if not valid:
        print("Invalid formula")
        sys.exit(1)
    def treedepth_helper(node):
        if len(node.children) == 1:
            node = node.children[0]
            if node.data == "prop_cons":
                return 1
            elif node.data == "prop_var":
                return 1
            elif node.data == "wff":
                return treedepth_helper(node)
        elif node.children[1].data == "binary_prop_conn":
            return max(treedepth_helper(node.children[0]), 
                       treedepth_helper(node.children[2])) + 1
        elif node.children[0].data == "unary_temp_conn":
            return treedepth_helper(node.children[2]) + 1
        elif node.children[1].data == "binary_temp_conn":
            return max(treedepth_helper(node.children[0]),
                       treedepth_helper(node.children[3])) + 1
    return treedepth_helper(tree)

def get_n(formula: str) -> int:
    '''
    Input  
        formula: a MLTL formula
    Output
        n: the number of propositional variables in the formula
    '''
    propvars = re.findall("p\d+", formula)
    propvars = [int(prop[1:]) for prop in propvars]
    return max(propvars) + 1

def scale_intervals(formula: str, MAX: int) -> str:
    '''
    Input
        formula: a MLTL formula
        MAX: the maximum bound for intervals
    Output
        formula: the formula with scaled intervals
    Computes the ratio of the largest number in all intervals to MAX
    Scales all intervals by the ratio
    Guarantees that the largest number in all intervals is equal to MAX
    '''
    # find all intervals
    intervals = re.findall("\[\s*\d+\s*,\s*\d+\s*\]", formula)
    # find largest number in all intervals
    max_bound = 0
    for interval in intervals:
        r1 = [int(num) for num in re.findall("\d+", interval)]
        max_bound = max(max_bound, max(r1))

    # find all numbers in interval
    for interval in intervals:
        r1 = [int(num) for num in re.findall("\d+", interval)]
        # scale interval
        r1 = [num * MAX / max_bound for num in r1]
        # replace interval
        formula = formula.replace(interval, f"[{int(r1[0])},{int(r1[1])}]")
    return formula

def write_traces_to_dir(traces: list, directory: str):
    '''
    Input
        traces: a list of traces
        directory: the directory to write the traces to
    Each file will be named the index of the trace
    '''
    if not os.path.exists(directory):
        os.makedirs(directory)
    for i, trace in enumerate(traces):
        with open(os.path.join(directory, f"{i}.txt"), 'w') as f:
            for line in trace:
                f.write(",".join(list(line)) + "\n")

if __name__ == '__main__':
    print("="*50)
    # SAMPLE USAGE FOR interpret
    formula = "G[0,3] (p0 | p1)"
    trace =["01", "11", "10", "00"]
    verdict = interpret(formula, trace) 
    print("Sample usage for interpret")
    print(f"Formula: {formula}")
    print(f"Trace: {trace}")
    print(f"Verdict: {verdict}")
    # True
    print("="*50) 

    # SAMPLE USAGE FOR interpret_batch with directory
    formula = "G[0,3] (p0 | p1)"
    traces_dir = os.path.join('MLTL_interpreter', 'traces') # look at ./MLTL_interpreter/traces
    results = interpret_batch(formula, traces_dir)
    print("Sample usage for interpret_batch with directory")
    print(f"Formula: {formula}")
    print(f"Traces directory: {traces_dir}")
    pprint(results)
    # {'trace1.csv': True, 
    #  'trace2.csv': False, 
    #  'trace3.csv': True}
    print("="*50)

    # SAMPLE USAGE FOR interpret_batch with list of traces
    formula = "G[0,3] (p0 | p1)"
    traces = [["01", "11", "01", "11"], 
              ["01", "10", "11", "00"],
              ["11", "11", "11", "11"],
              ]
    results = interpret_batch(formula, traces)
    print("Sample usage for interpret_batch with list of traces")
    print(f"Formula: {formula}")
    print(f"Traces:")
    [print(i, t) for i, t in enumerate(traces)]
    pprint(results)
    # {0: True,
    #  1: False,
    #  2: True}
    print("="*50)

    # SAMPLE USAGE FOR west
    formula = "G[0,3] (p0 & p1)"
    trace_regexes = west(formula)
    print("Sample usage for west")
    print(f"Formula: {formula}")
    pprint(trace_regexes)
    # ['11,11,11,11']
    print("="*50)

    # SAMPLE USAGE FOR random_trace
    m_delta = 4
    trace = random_trace(trace_regexes, m_delta)
    print("Sample usage for random_trace")
    print(f"Random trace: {trace}")
    # non-deterministic
    print("="*50)

    # SAMPLE USAGE FOR comp_len
    formula = "((G[0,2]!p0) R[0,3] (F[0,3]p2))"
    complen = comp_len(formula)
    print("Sample usage for comp_len")
    print(f"Formula: {formula}")
    print(f"Computation length: {complen}")
    # 7 = 3 + max(2, 3) + 1 = 7
    print("="*50)

    # SAMPLE USAGE FOR treedepth
    formula = "((G[0,2]!p0) R[0,3] (F[0,3](p1 -> p2)))"
    depth = treedepth(formula)
    print("Sample usage for treedepth")
    print(f"Formula: {formula}")
    print(f"Tree depth: {depth}")
    # 4
    print("="*50)

    # SAMPLE USAGE FOR get_n
    formula = "((G[0,2]!p0) R[0,3] (F[0,3](p1 -> p2)))"
    n = get_n(formula)
    print("Sample usage for get_n")
    print(f"Formula: {formula}")
    print(f"Number of propositional variables: {n}")
    # 3
    print("="*50)

    # SAMPLE USAGE FOR scale_intervals
    formula = "(G[50,100]p0) & (F[40,200]p1)"
    MAX = 20
    scaled_formula = scale_intervals(formula, MAX)
    print("Sample usage for scale_intervals")
    print(f"Formula: {formula}")
    print(f"Max bound: {MAX}")
    print(f"Scaled formula: {scaled_formula}")
    # (G[5,10]p0) & (F[2,20]p1)
    print("="*50)
