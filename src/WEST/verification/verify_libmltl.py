# Author: Zili Wang
# Date Created: 02/12/2024
# Verify that WEST and mltl_lib produce the same output

import subprocess
import time
import os
import sys
from string_src.parser import from_west
import re
from verify_r2u2 import compare_files, iterate_traces, get_n
import libmltl as mltl

def run_west(formula):
    west_exec = "./west"
    subprocess.run(f"cd .. && {west_exec} \"{formula}\" cd ./verification", 
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, shell=True)

def run_interpreter(formula, traces, verbose=False):
    ast = mltl.parse(formula)
    sat_traces = []
    for trace in traces:
        if ast.evaluate(trace):
            sat_traces.append(trace)
    with open("./libmltl_output/output.txt", "w") as f:
        f.write(f"Formula: {formula}\n")
        for trace in sat_traces:
            f.write(",".join(trace) + "\n")

def get_mn():
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
            if n > 0:
                m = 1
            elif n == 0:
                m, n = 1, 5
    return m, n

def verify(formula):
    start = time.perf_counter()
    run_west(formula)
    m, n = get_mn()
    print(f"Verifying formula \"{formula}\" with {m} time steps and {n} variables")
    traces = list(iterate_traces(m, n))
    run_interpreter(formula, traces)
    # compare the files
    with open("../output/output.txt", "r") as f1:
        with open("./libmltl_output/output.txt", "r") as f2:
            if compare_files(f1, f2):
                total = time.perf_counter() - start
                print(f"Output files are identical on formula \"{formula}\", verified in {total} seconds\n")
                return True
            else:
                print(f"Output files are different on formula \"{formula}\"\n")
                return False

if __name__ == "__main__":
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
            if not verify(formula):
                exit(1)
    
    # if we get here, all formulas were verified
    print("CONGRATULATIONS! ALL FORMULAS WERE VERIFIED!")