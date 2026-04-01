import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os

sns.set_theme(style="whitegrid", context="paper")
plt.rcParams.update({
    "font.family": "serif",
    "font.serif": ["Times New Roman", "Times"],
    "font.size": 10,
    "axes.labelsize": 10,
    "xtick.labelsize": 9,
    "ytick.labelsize": 9,
    "legend.fontsize": 9,
    "pdf.fonttype": 42,
    "ps.fonttype": 42
})

def plot_attack_success():
    csv_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../phase4_ns3_simulation/results/apr2_3_team_c_metrics.csv")
    if not os.path.exists(csv_path):
        print(f"Error: Data file {csv_path} not found.")
        return

    df = pd.read_csv(csv_path)
    
    # Filter for attack effect
    df_attack = df[(df['metric'] == 'attack_effect_percent') & (df['attack'] != 'none')].copy()
    
    df_attack['topology'] = df_attack['topology'].str.capitalize()
    df_attack['attack'] = df_attack['attack'].str.capitalize()
    
    patch_mapping = {
        'baseline': 'Baseline',
        'metricversion': 'Metric-Only',
        'patched': 'Patched (Proposed)'
    }
    df_attack['patch_version'] = df_attack['patch_version'].map(patch_mapping)

    fig, axes = plt.subplots(1, 2, figsize=(7, 3), sharey=True)
    palette = {"Baseline": "#D62728", "Metric-Only": "#2CA02C", "Patched (Proposed)": "#1F77B4"}

    attacks = ['Spoofing', 'Replay']
    for i, attack in enumerate(attacks):
        ax = axes[i]
        subset = df_attack[df_attack['attack'] == attack]
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
        ax.set_title(f"{attack} Attack")
        ax.set_xlabel("Network Topology")
        if i == 0:
            ax.set_ylabel("Attack Success Rate (%)")
        else:
            ax.set_ylabel("")
            
        ax.set_ylim(-5, 120)
        ax.grid(True, axis='y', linestyle='--', alpha=0.7, zorder=0)
        
        # Add values on top of bars
        for container in ax.containers:
            labels = [f'{v.get_height():.1f}%' if v.get_height() > 0 else '0%' for v in container]
            ax.bar_label(container, labels=labels, padding=3, size=8, rotation=90)
            
        if i == 1:
            ax.legend(title="", loc='upper right', framealpha=0.9, bbox_to_anchor=(1, 1))
        else:
            if ax.get_legend():
                ax.get_legend().remove()

    plt.tight_layout()
    output_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig3_comparison.pdf")
    plt.savefig(output_path, format="pdf", dpi=300, bbox_inches="tight")
    print(f"Saved {output_path}")
    plt.close()

if __name__ == "__main__":
    plot_attack_success()
