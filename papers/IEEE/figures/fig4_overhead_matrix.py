import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import numpy as np

sns.set_theme(style="whitegrid", context="paper")
plt.rcParams.update({
    "font.family": "serif",
    "font.serif": ["Times New Roman", "Times", "DejaVu Serif"],
    "font.size": 10,
    "axes.labelsize": 10,
    "xtick.labelsize": 9,
    "ytick.labelsize": 9,
    "legend.fontsize": 9,
    "pdf.fonttype": 42,
    "ps.fonttype": 42
})

def plot_overhead():
    csv_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../phase4_ns3_simulation/results/apr2_3_team_c_metrics.csv")
    if not os.path.exists(csv_path):
        print(f"Error: Data file {csv_path} not found.")
        return

    df = pd.read_csv(csv_path)
    
    # Filter for no attack (normal operation)
    df_normal = df[df['attack'] == 'none'].copy()
    
    # We want pdr_percent, latency_ms, throughput_pkt_per_sec
    metrics_of_interest = ['pdr_percent', 'latency_ms', 'throughput_pkt_per_sec']
    df_normal = df_normal[df_normal['metric'].isin(metrics_of_interest)]
    
    df_normal['topology'] = df_normal['topology'].str.capitalize()
    
    patch_mapping = {
        'baseline': 'Baseline',
        'patched': 'Patched (Proposed)'
    }
    # Keep only baseline and patched
    df_normal = df_normal[df_normal['patch_version'].isin(['baseline', 'patched'])]
    df_normal['patch_version'] = df_normal['patch_version'].map(patch_mapping)

    fig, axes = plt.subplots(1, 3, figsize=(10, 3))
    palette = {"Baseline": "#D62728", "Patched (Proposed)": "#1F77B4"}

    metric_configs = {
        'pdr_percent': ('Packet Delivery Ratio (%)', axes[0]),
        'latency_ms': ('Latency (ms)', axes[1]),
        'throughput_pkt_per_sec': ('Throughput (pkt/s)', axes[2])
    }

    for metric, (ylabel, ax) in metric_configs.items():
        subset = df_normal[df_normal['metric'] == metric]
        
        sns.barplot(
            data=subset, 
            x='topology', 
            y='value', 
            hue='patch_version',
            palette=palette,
            ax=ax,
            edgecolor='black',
            linewidth=1,
            zorder=3
        )
        ax.set_title(ylabel)
        ax.set_xlabel("Network Topology")
        ax.set_ylabel("")
        ax.grid(True, axis='y', linestyle='--', alpha=0.7, zorder=0)
        
        if metric == 'pdr_percent':
            ax.set_ylim(0, 115)
            for container in ax.containers:
                labels = [f'{v.get_height():.0f}%' for v in container]
                ax.bar_label(container, labels=labels, padding=3, size=8)
        elif metric == 'latency_ms':
            ymax = subset['value'].max() * 1.2
            ax.set_ylim(0, ymax)
            for container in ax.containers:
                labels = [f'{v.get_height():.1f}' for v in container]
                ax.bar_label(container, labels=labels, padding=3, size=8)
        elif metric == 'throughput_pkt_per_sec':
            ymax = subset['value'].max() * 1.2
            ax.set_ylim(0, ymax)
            for container in ax.containers:
                labels = [f'{v.get_height():.3f}' for v in container]
                ax.bar_label(container, labels=labels, padding=3, size=8)
                
        if metric == 'throughput_pkt_per_sec':
            ax.legend(title="", loc='upper right', framealpha=0.9, bbox_to_anchor=(1, 1))
        else:
            if ax.get_legend():
                ax.get_legend().remove()

    plt.tight_layout()
    output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig4_overhead_matrix.pdf")
    plt.savefig(output_path, format="pdf", dpi=300, bbox_inches="tight")
    print(f"Saved {output_path}")
    plt.close()

if __name__ == "__main__":
    plot_overhead()
