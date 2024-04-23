import os
import sys
from pprint import pprint
sys.path.append(os.path.dirname(os.path.realpath(__file__)) + '/../libmltl/lib')
import libmltl as mltl

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

