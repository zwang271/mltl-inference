import bnlearn as bn
import pandas as pd
import matplotlib.pyplot as plt
from utils import *

if __name__ == "__main__":
    df = pd.read_csv("data.txt")
    model = bn.structure_learning.fit(df)
    print(model['adjmat'])
    G = bn.plot(model)


