# Author: Zili Wang
# Last updated: 02/08/2024
# Verify that WEST and r2u2 produce the same output
# This can only be ran on a Posix environment (Linux, MacOS, Etc.)

import subprocess
import time
import os
import sys
from string_src.parser import from_west
import re

def run_west(formula):
    west_exec = "./west"
    subprocess.run(f"cd .. && {west_exec} \"{formula}\" cd ./verification", 
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, shell=True)

def run_r2u2(formula, trace: list[str], verbose=False):
    # formula = formula.replace("|", "||").replace("&", "&&")
    formula = formula.replace("p", "a")
    m = len(trace)
    n = len(trace[0]) if m > 0 else 0

    # define file paths
    c2po = "../../compiler/c2po.py"
    spec_mltl = "./r2u2_output/spec.mltl"
    trace_csv = "./r2u2_output/trace.csv"
    r2u2_spec_bin = "./spec.bin"
    r2u2 = "../../monitors/static/build/r2u2"

    VARS = ", ".join([f"a{i}" for i in range(n)])
    with open(spec_mltl, "w") as f:
        f.write(formula+"\n")
    with open(trace_csv, "w") as f:
        VARS = VARS.replace(" ", "")
        f.write(f"# {VARS}\n")
        for t in trace:
            f.write(",".join(list(t)) + "\n")
    cmd1 = f"python3 {c2po} --booleanizer {spec_mltl}"
    subprocess.run(cmd1, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, shell=True)
    cmd2 = f"{r2u2} {r2u2_spec_bin} {trace_csv}"
    if verbose:
        subprocess.run(cmd2, shell=True)
        return
    output = subprocess.run(cmd2, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    output = output.stdout.decode("utf-8")
    output = output.split("\n")
    output = output[0][-1]
    return True if output == "T" else False

# Replaces all s in the string with all permutations of 0 and 1
def expand_string(string):
    def expand_string_helper(string):
        if 's' not in string:
            return {string}
        else:
            return expand_string_helper(string.replace('s', '0', 1))\
                .union(expand_string_helper(string.replace('s', '1', 1)))
    return expand_string_helper(string)

# compares if f1 and f2 are equivalent
def compare_files(f1, f2):
    # skip the first line
    f1.readline()
    f2.readline()

    # turn each file into a list 
    f1_name = f1.name
    f2_name = f2.name
    f1 = [line.strip() for line in list(f1)]
    f2 = [line.strip() for line in list(f2)]

    # degenerate cases involving empty files
    if len(f1) == 0 and len(f2) == 0:
        return True
    elif len(f1) == 0 and len(f2) != 0:
        return False
    elif len(f1) != 0 and len(f2) == 0:
        return False
    
    expanded1 = set()
    expanded2 = set()
    # check that the expanded strings are the same
    for line in f1:
        for w in expand_string(line):
            expanded1.add(w)
    for line in f2:
        for w in expand_string(line):
            expanded2.add(w)

    # turn each sets into a list sorted by lex order
    expanded1 = sorted(list(expanded1))
    expanded2 = sorted(list(expanded2))
    if expanded1 == expanded2:
        return True
    
    # print out the differences 
    for i, line in enumerate(f1):
        if line not in f2:
            print(f"Line {i+2} of {f1_name} is not in {f2_name}")
    for i, line in enumerate(f2):
        if line not in f1:
            print(f"Line {i+2} of {f2_name} is not in {f1_name}")
    return False

# iterate through all traces of n variables and m time steps
def iterate_traces(m, n):
    for i in range(2**(m*n)):
        t = str(bin(i)[2:])
        t = "0" * (m*n - len(t)) + t
        t = [t[i*n:(i+1)*n] for i in range(m)]
        yield t

def verify(formula):
    start = time.perf_counter()
    run_west(formula)
    with open("../output/output.txt", "r") as f:
        regex = f.readlines()
        regex = regex[1] if len(regex) > 1 else None
        if regex is not None:
            regex = regex.split(",") if "," in regex else [regex.strip()]
            m = len(regex)
            n = len(regex[0]) if m > 0 else 0
        else:
            # in formula, find all instances of p0, p1, p2, etc. and find the max
            n = get_n(formula)
            m = min(5, 18//n)
        
    print(f"Verifying formula \"{formula}\" with {m} time steps and {n} variables")
    with open("./r2u2_output/output.txt", "w") as f:
        f.write(formula + "\n")
        for trace in iterate_traces(m, n):
            if run_r2u2(formula, trace):
                trace = ",".join(trace)
                f.write(trace + "\n")
    with open("../output/output.txt", "r") as f1:
        with open("./r2u2_output/output.txt", "r") as f2:
            if compare_files(f1, f2):
                total = time.perf_counter() - start
                print(f"Output files are identical on formula \"{formula}\", verified in {total} seconds\n")
                return True
            else:
                print(f"Output files are different on formula \"{formula}\"\n")
                return False

def get_n(formula):
    n = -1
    for match in re.finditer(r"p\d+", formula):
        n = max(n, int(match.group(0)[1:]))
    return n+1

if __name__ == '__main__':
    # if given a formula as an argument, use that as checkpoint
    checkpoint = None
    if len(sys.argv) > 1:
        formula = sys.argv[1].strip()
        checkpoint = formula

    # verify all formulas in verify_formulas, skip up to checkpoint if checkpoint is given
    # checkpoint = "F[0:2]G[0:2]p3"
    skip = False if checkpoint is None else True
    with open(f"./verify_formulas/formulas.txt", "r") as f:
        for line in f:
            formula = line.strip()
            if skip:
                print(f"Skipping verification for formula \"{formula}\", before checkpoint\n")
                if formula == checkpoint:
                    skip = False
                continue
            print(formula)
            if get_n(formula) == 0:
                print("r2u2 cannot handle formulas with no variables\n")
                continue
            if not verify(formula):
                exit(1)
    
    # if we get here, all formulas were verified
    print("CONGRATULATIONS! ALL FORMULAS WERE VERIFIED!")
        