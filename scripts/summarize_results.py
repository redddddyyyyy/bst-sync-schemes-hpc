import pandas as pd

df = pd.read_csv("results_raw.csv")

group_cols = ["mode", "tree", "n_init", "n_search", "threads"]
agg = df.groupby(group_cols).agg(
    elapsed_mean=("elapsed_s", "mean"),
    elapsed_std=("elapsed_s", "std"),
    thr_mean=("throughput_mops", "mean"),
    thr_std=("throughput_mops", "std"),
    ins_mean=("instructions_total", "mean"),
    cyc_mean=("cycles_total", "mean"),
).reset_index()

# Optional: derived metrics
agg["ipc"] = agg["ins_mean"] / agg["cyc_mean"]

agg.to_csv("results.csv", index=False)
print("Wrote results.csv")
