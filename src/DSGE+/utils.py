import os
import sys
import tempfile
import subprocess
from pprint import pprint
import random
import re
import mltl_parser
sys.path.append(os.path.dirname(os.path.realpath(__file__)) + '/../libmltl/lib')
import libmltl as mltl

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
            trace = [line.strip() for line in f]
            traces.append(trace)
    return traces

if __name__ == '__main__':
    print("="*50)
    # SAMPLE USAGE FOR interpret
    formula = "G[0,3] (p0 | p1)"
    ast = mltl.parse("G[0,3] (p0 | p1)")
    trace =["01", "11", "10", "00"]
    verdict = ast.evaluate(trace)
    print("Sample usage for interpret")
    print(f"Formula: {formula}")
    print(f"Trace: {trace}")
    print(f"Verdict: {verdict}")
    # True
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
