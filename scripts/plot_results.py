import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("results.csv")

# Throughput vs threads
for tree in sorted(df["tree"].unique()):
    sub = df[df["tree"] == tree]
    for mode in ["seq", "cg", "fg", "ideal"]:
        m = sub[sub["mode"] == mode].sort_values("threads")
        if m.empty: 
            continue
        plt.plot(m["threads"], m["thr_mean"], marker="o", label=f"{mode}-{tree}")
    plt.xlabel("Threads")
    plt.ylabel("Throughput (M ops/s)")
    plt.title(f"Throughput vs Threads (tree={tree})")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(f"throughput_vs_threads_{tree}.png", dpi=200)
    plt.clf()

# Speedup vs threads (baseline = seq @ 1 thread)
baseline = df[(df["mode"]=="seq") & (df["threads"]==1)].copy()
for tree in sorted(df["tree"].unique()):
    b = baseline[baseline["tree"]==tree]
    if b.empty:
        continue
    base_time = float(b["elapsed_mean"].iloc[0])

    sub = df[df["tree"]==tree].copy()
    sub["speedup"] = base_time / sub["elapsed_mean"]

    for mode in ["seq","cg","fg","ideal"]:
        m = sub[sub["mode"]==mode].sort_values("threads")
        if m.empty: 
            continue
        plt.plot(m["threads"], m["speedup"], marker="o", label=mode)
    plt.xlabel("Threads")
    plt.ylabel("Speedup vs seq(1 thread)")
    plt.title(f"Speedup vs Threads (tree={tree})")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(f"speedup_vs_threads_{tree}.png", dpi=200)
    plt.clf()

print("Wrote plots.")
