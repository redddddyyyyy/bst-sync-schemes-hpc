import pandas as pd
import matplotlib.pyplot as plt

N_SEARCH = 5_000_000

df = pd.read_csv("results.csv")

# Compute throughput
df["throughput"] = N_SEARCH / df["elapsed"]

# Compute speedup per mode relative to its T=1
speedups = []
for mode in df["mode"].unique():
    mask = df["mode"] == mode
    t1 = df.loc[mask & (df["threads"] == 1), "elapsed"].iloc[0]
    speedups.extend(list(t1 / df.loc[mask, "elapsed"]))
df["speedup"] = speedups

print(df)

# Throughput vs threads
plt.figure()
for mode in df["mode"].unique():
    sub = df[df["mode"] == mode].sort_values("threads")
    plt.plot(sub["threads"], sub["throughput"], marker="o", label=mode)

plt.xlabel("Threads")
plt.ylabel("Throughput (ops/sec)")
plt.title("Throughput vs Threads (bal tree)")
plt.grid(True)
plt.xscale("log", base=2)
plt.legend()
plt.tight_layout()
plt.savefig("throughput_vs_threads_all_modes.png")

# Speedup vs threads
plt.figure()
for mode in df["mode"].unique():
    sub = df[df["mode"] == mode].sort_values("threads")
    plt.plot(sub["threads"], sub["speedup"], marker="o", label=mode)

plt.xlabel("Threads")
plt.ylabel("Speedup (T1 / T)")
plt.title("Speedup vs Threads (bal tree)")
plt.grid(True)
plt.xscale("log", base=2)
plt.legend()
plt.tight_layout()
plt.savefig("speedup_vs_threads_all_modes.png")

print("Saved throughput_vs_threads_all_modes.png and speedup_vs_threads_all_modes.png")

