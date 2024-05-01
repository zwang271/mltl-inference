import subprocess
import pathlib
import os

for dataset in os.listdir(pathlib.Path("../dataset")):
    dataset_path = pathlib.Path(f"../dataset/{dataset}")
    #  python3 traces2json.py ../dataset/basic_future/
    subprocess.run(["python3", "traces2json.py", str(dataset_path)])
