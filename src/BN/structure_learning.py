import bnlearn as bn
import pandas as pd
import matplotlib.pyplot as plt
from utils import load_traces
import pathlib

def to_csv(data, label):
    pass

if __name__ == "__main__":
    # df = pd.read_csv("data.csv", sep=",", skipinitialspace=True)
    # strip all white spaces from columns and rows
    # df = df.apply(lambda x: x.str.strip() if x.dtype == "object" else x)
    # fit the structure of the data
    # model = bn.structure_learning.fit(df)
    # plot the model
    # bn.plot(model)    

    dataset = pathlib.Path("../dataset/basic_future")
    # load traces
    pos_train = load_traces(dataset / "pos_train")
    neg_train = load_traces(dataset / "neg_train")
    n = len(pos_train[0][0])
    vars = [f"p{i}" for i in range(n)] + [f"p{i}'" for i in range(n)] + ["v"]
    print(vars)



