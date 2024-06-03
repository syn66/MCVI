#!/usr/bin/python3

import numpy as np
from CTP_generator import generate_delaunay_graph_set, graph_to_cpp
from evaluation import initialise_folder
from time_series import parse_file
import subprocess
import pandas as pd
import time
import pickle

TIMEOUT = 60 * 60 * 10

timestr = time.strftime("%Y-%m-%d_%H-%M")

PROBLEM_SIZE = 15
SET_SIZE = 10

RESULTS_FOLDER = f"eval_results_{PROBLEM_SIZE}x{SET_SIZE}_{timestr}"
GRAPH_FILE = "experiments/CTP/auto_generated_graph.h"


def run_ctp_instance(N, i):
    outfile = f"{RESULTS_FOLDER}/CTPInstance_{N}_{i}.txt"

    # Build files
    cmd = "cd build && make"
    p = subprocess.run(
        cmd,
        stderr=subprocess.PIPE,
        stdout=subprocess.PIPE,
        shell=True,
    )
    if p.returncode != 0:
        print(p.stdout)
        print(p.stderr)
        raise RuntimeError(f"Build failed with returncode {p.returncode}")

    with open(outfile, "w") as f:
        # Run solver
        cmd = f"time build/experiments/CTP/ctp_timeseries"
        p = subprocess.run(
            cmd,
            stdout=f,
            stderr=subprocess.PIPE,
            timeout=TIMEOUT,
            shell=True,
        )
        if p.returncode != 0:
            print(f"INSTANCE {N}_{i} FAILED")
            print(p.stderr)
            return outfile, 1
        else:
            print(p.stderr, file=f)

        # Copy problem instance to outfile
        f.write("\n")
        with open(GRAPH_FILE, "r") as f1:
            f.write(f1.read())
    return outfile, 0


if __name__ == "__main__":
    seed = np.random.randint(0, 9999999)

    initialise_folder(RESULTS_FOLDER)

    results = []
    headers_written = False

    problem_graphs = generate_delaunay_graph_set(PROBLEM_SIZE, SET_SIZE, seed)
    with open(f"{RESULTS_FOLDER}/problem_graphs.pickle", "wb") as f:
        pickle.dump(problem_graphs, f)

    for i, (G, origin, goal, seed) in enumerate(problem_graphs):
        with open(GRAPH_FILE, "w") as f:
            graph_to_cpp(G, origin, goal, f)

        outfile, error = run_ctp_instance(PROBLEM_SIZE, i)
        if error:
            continue

        # Summarise results
        instance_result = parse_file(outfile)
        instance_result["Seed"] = seed
        instance_result["Set number"] = i
        results.append(instance_result)

        # Save as we go
        df = instance_result
        if not headers_written:
            df.to_csv(f"{RESULTS_FOLDER}/ctp_results.csv", index=False)
            headers_written = True
        else:
            df.to_csv(
                f"{RESULTS_FOLDER}/ctp_results.csv",
                mode="a",
                index=False,
                header=False,
            )

    df = pd.concat(results, ignore_index=True, sort=False)
    df.to_csv(f"{RESULTS_FOLDER}/ctp_results_all.csv", index=False)