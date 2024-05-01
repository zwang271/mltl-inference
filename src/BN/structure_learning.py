import bnlearn as bn
import pandas as pd
import matplotlib.pyplot as plt
import pathlib
from utils import *

if __name__ == "__main__":
    dataset = pathlib.Path("../dataset/fmsd17_formula1/")
    pos_train = load_traces(dataset / "pos_train/")
    neg_train = load_traces(dataset / "neg_train/")
    pos_test = load_traces(dataset / "pos_test/")   
    neg_test = load_traces(dataset / "neg_test/")

    for trace in neg_train:
        print(len(trace))


