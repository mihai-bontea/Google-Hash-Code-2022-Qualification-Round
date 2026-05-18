from collections import Counter, defaultdict

import matplotlib.gridspec as gridspec
import matplotlib.pyplot as plt
import numpy as np

def parse_input(filepath):
    with open(filepath) as f:
        lines = iter(f.read().splitlines())

    C, P = map(int, next(lines).split())

    contributors = {}
    for _ in range(C):
        name, n = next(lines).split()
        contributors[name] = {
            s: int(lvl)
            for _ in range(int(n))
            for s, lvl in [next(lines).split()]
        }

    projects = []
    for _ in range(P):
        name, D, S, B, R = next(lines).split()
        roles = [
            (s, int(lvl))
            for _ in range(int(R))
            for s, lvl in [next(lines).split()]
        ]
        projects.append({
            "name": name,
            "duration": int(D),
            "score": int(S),
            "best_before": int(B),
            "roles": roles,
        })

    skills = {s for m in contributors.values() for s in m}
    for m in contributors.values():
        m.update({s: 0 for s in skills - m.keys()})

    return C, P, contributors, projects


def compute_metrics(contributors, projects):
    # Available contributor levels per skill
    supply_levels = defaultdict(list)
    for skills in contributors.values():
        for s, lvl in skills.items():
            supply_levels[s].append(lvl)

    # Required levels per skill across all projects
    demand_levels = defaultdict(list)
    for p in projects:
        for sname, slvl in p["roles"]:
            demand_levels[sname].append(slvl)

    all_skills = set(supply_levels) | set(demand_levels)

    # Max observed level per skill (supply or demand)
    max_level_per_skill = {}
    for s in all_skills:
        m_sup = max(supply_levels.get(s, [0]), default=0)
        m_dem = max(demand_levels.get(s, [0]), default=0)
        max_level_per_skill[s] = max(m_sup, m_dem)

    # Scarcity = weighted demand / qualified supply
    scarcity = {}
    for s in all_skills:
        sup = supply_levels.get(s, [])
        dem = demand_levels.get(s, [])

        if not dem:
            scarcity[s] = 0.0
            continue

        score = 0.0
        dem_counter = Counter(dem)

        for L, count_at_L in dem_counter.items():
            sup_ge_L = sum(1 for x in sup if x >= L)
            score += count_at_L * L / max(1, sup_ge_L)

        scarcity[s] = score

    # Basic project stats
    slacks    = [p["best_before"] - p["duration"] for p in projects]
    scores    = [p["score"] for p in projects]
    durs      = [p["duration"] for p in projects]
    bestbef   = [p["best_before"] for p in projects]
    densities = [p["score"] / max(1, p["duration"]) for p in projects]

    # Role rarity = level / qualified contributors
    def role_rarity(sname, slvl):
        sup = supply_levels.get(sname, [])
        sup_ge_L = sum(1 for x in sup if x >= slvl)
        return slvl / max(1, sup_ge_L)

    # Project rarity = sum of role rarities
    project_rarity = [
        sum(role_rarity(s, L) for s, L in p["roles"])
        for p in projects
    ]

    # Net value = score density penalized by rarity
    pos_dens = [d for d in densities if d > 0]
    pos_rar  = [r for r in project_rarity if r > 0]

    if pos_dens and pos_rar:
        med_dens = float(np.median(pos_dens))
        med_rar  = float(np.median(pos_rar))
        alpha = 0.3 * med_dens / med_rar
    else:
        alpha = 0.0

    net_value = [
        d - alpha * r
        for d, r in zip(densities, project_rarity)
    ]

    # Staffing pressure = competing demand / supply
    demand_count_by_skill_level = defaultdict(Counter)
    for pp in projects:
        for ss, LL in pp["roles"]:
            demand_count_by_skill_level[ss][LL] += 1

    role_congestion_cache = {}
    distinct_role_keys = {
        (s, L)
        for p in projects
        for s, L in p["roles"]
    }

    for (s, L) in distinct_role_keys:
        sup = supply_levels.get(s, [])
        supply_count = sum(1 for x in sup if x >= L)

        demand_count = sum(
            cnt
            for lvl, cnt in demand_count_by_skill_level[s].items()
            if lvl >= L
        )

        if supply_count == 0:
            role_congestion_cache[(s, L)] = float(demand_count)
        else:
            role_congestion_cache[(s, L)] = (
                demand_count / supply_count
            )

    staffing_pressure = []
    for p in projects:
        total = 0.0
        for s, L in p["roles"]:
            total += role_congestion_cache[(s, L)]
        staffing_pressure.append(total)

    # Mentorship leverage: L-1 contributors vs direct matches
    mentorship_unlock = {}
    for s in all_skills:
        sup = supply_levels.get(s, [])
        dem = demand_levels.get(s, [])

        direct = 0
        via_mentor = 0

        for L in dem:
            direct     += sum(1 for x in sup if x >= L)
            via_mentor += sum(1 for x in sup if x == L - 1)

        mentorship_unlock[s] = (via_mentor, direct)

    return {
        "supply_levels":       supply_levels,
        "demand_levels":       demand_levels,
        "all_skills":          all_skills,
        "scarcity":            scarcity,
        "slacks":              slacks,
        "scores":              scores,
        "durations":           durs,
        "best_before":         bestbef,
        "densities":           densities,
        "project_rarity":      project_rarity,
        "net_value":           net_value,
        "alpha":               alpha,
        "staffing_pressure":   staffing_pressure,
        "mentorship_unlock":   mentorship_unlock,
        "max_level_per_skill": max_level_per_skill,
        "projects":            projects,
    }

BG      = "#1a1a2e"
PANEL   = "#16213e"
ACCENT  = "#0f3460"
CYAN    = "#00d4ff"
MAGENTA = "#e94560"
YELLOW  = "#f5a623"
GREEN   = "#4ecca3"
WHITE   = "#e0e0e0"
GRAY    = "#888888"


def style_ax(ax, title):
    ax.set_facecolor(PANEL)
    ax.tick_params(colors=WHITE, labelsize=8)
    ax.xaxis.label.set_color(WHITE)
    ax.yaxis.label.set_color(WHITE)
    ax.title.set_color(WHITE)
    ax.set_title(title, fontsize=10, fontweight="bold", pad=8)
    for spine in ax.spines.values():
        spine.set_edgecolor(ACCENT)


def plot_supply_vs_demand(ax, m, n=20):
    """
    Top-N skills by total demand, paired bars: contributors who have the skill
    at level >= 1 vs total roles requiring it.
    """
    skills = sorted(m["all_skills"],
                    key=lambda s: -len(m["demand_levels"].get(s, [])))[:n]
    sup = [sum(1 for x in m["supply_levels"].get(s, []) if x >= 1)
           for s in skills]
    dem = [len(m["demand_levels"].get(s, [])) for s in skills]

    y = np.arange(len(skills))
    h = 0.4
    ax.barh(y - h/2, sup, h, color=GREEN,   label="contributors with skill ≥1")
    ax.barh(y + h/2, dem, h, color=MAGENTA, label="roles requiring skill")
    ax.set_yticks(y)
    ax.set_yticklabels(skills, fontsize=7)
    ax.invert_yaxis()
    ax.set_xlabel("Count")
    style_ax(ax, f"Top {len(skills)} Skills — Supply vs Demand")
    ax.legend(fontsize=7, facecolor=PANEL, labelcolor=WHITE, framealpha=0.7,
              loc="lower right")


def plot_level_heatmap(ax, m, n=15):
    """
    For top-N most-demanded skills, a heatmap of (skill x level) showing
    supply(>=L) minus demand(=L) at each level. Blue = surplus, red = shortage.
    Levels start at 1
    """
    skills = sorted(m["all_skills"],
                    key=lambda s: -len(m["demand_levels"].get(s, [])))[:n]
    if not skills:
        style_ax(ax, "Supply − Demand by Level")
        return

    max_lvl = max(m["max_level_per_skill"][s] for s in skills)
    max_lvl = max(max_lvl, 1)

    matrix = np.zeros((len(skills), max_lvl))  # columns = levels 1..max_lvl
    for i, s in enumerate(skills):
        sup = m["supply_levels"].get(s, [])
        dem = m["demand_levels"].get(s, [])
        for L in range(1, max_lvl + 1):
            sup_ge_L = sum(1 for x in sup if x >= L)
            dem_at_L = sum(1 for x in dem if x == L)
            matrix[i, L - 1] = sup_ge_L - dem_at_L

    vmax = np.abs(matrix).max() if matrix.size else 1
    im = ax.imshow(matrix, cmap="RdBu", aspect="auto",
                   vmin=-vmax, vmax=vmax)
    ax.set_xticks(range(max_lvl))
    ax.set_xticklabels(range(1, max_lvl + 1), fontsize=7)
    ax.set_yticks(range(len(skills)))
    ax.set_yticklabels(skills, fontsize=7)
    ax.set_xlabel("Skill level")
    style_ax(ax, "Supply (≥L) - Demand (=L)   red = shortage")
    cb = plt.colorbar(im, ax=ax, fraction=0.04, pad=0.02)
    cb.ax.tick_params(labelcolor=WHITE, labelsize=7)


def plot_scarcity_ranking(ax, m, n=20):
    """
    Top-N skills by level-weighted scarcity score:
        Σ_L  demand_at_L * L / max(1, supply_>=_L)
    The level multiplier weights "expensive" demands higher. Skills near the
    top are the leverage points: get them staffed and many projects unlock.
    """
    items = sorted(m["scarcity"].items(), key=lambda x: -x[1])[:n]
    names  = [t[0] for t in items]
    vals   = [t[1] for t in items]
    colors = plt.cm.plasma(np.linspace(0.3, 1.0, len(names)))
    ax.barh(range(len(names)), vals, color=colors)
    ax.set_yticks(range(len(names)))
    ax.set_yticklabels(names, fontsize=7)
    ax.invert_yaxis()
    ax.set_xlabel("Scarcity  Σ (demand·L / supply≥L)")
    style_ax(ax, f"Top {len(names)} Scarcest Skills (level-weighted)")


def plot_slack_distribution(ax, m):
    """
    Project slack = best_before - duration
    Negative = impossible to finish on time even with day-0 start
    Small = tight scheduling, high = filler that can wait
    """
    slacks = m["slacks"]
    impossible = sum(1 for s in slacks if s < 0)
    tight      = sum(1 for s in slacks if 0 <= s < 5)
    avg        = float(np.mean(slacks)) if slacks else 0.0
    med        = float(np.median(slacks)) if slacks else 0.0

    ax.hist(slacks, bins=40, color=GREEN, edgecolor=BG, linewidth=0.4)
    ax.axvline(0, color=MAGENTA, linewidth=1.5, linestyle="--",
               label="Deadline")
    ax.axvline(avg, color=YELLOW, linewidth=1, linestyle=":",
               label=f"mean = {avg:.1f}")
    ax.set_xlabel("Slack = best_before − duration  (days)")
    ax.set_ylabel("Number of projects")
    style_ax(ax, "Project Slack Distribution")
    ax.legend(fontsize=7, facecolor=PANEL, labelcolor=WHITE, framealpha=0.7)
    ax.text(0.02, 0.95,
            f"Impossible on time: {impossible}  |  Tight (<5d): {tight}\n"
            f"median slack: {med:.1f}d",
            transform=ax.transAxes, color=YELLOW,
            ha="left", va="top", fontsize=8)


def plot_density_vs_rarity(ax, m):
    """
    The strategic tradeoff:

    X = score density (score / duration), the reward rate
    Y = project rarity cost (sum over roles of L / supply_>=_L), the
        scarcity 'tax' the project levies on shared resources
    Size = absolute score (bigger = more points on the table)
    Color = slack in days (cool = lots of slack, hot = deadline pressure)

    Diagonal dashed lines are iso-net-value contours
        density - alpha * rarity = const
    Projects above/left of a given line have lower net value than that
    line's intercept; projects below/right have higher.
    """
    dens   = np.asarray(m["densities"])
    rar    = np.asarray(m["project_rarity"])
    score  = np.asarray(m["scores"])
    slack  = np.asarray(m["slacks"])
    alpha  = m["alpha"]
    net    = np.asarray(m["net_value"])

    if len(dens) == 0:
        style_ax(ax, "Score Density vs Rarity Cost")
        return

    # Size: scale absolute scores to a visible range
    s_min, s_max = score.min(), score.max()
    if s_max > s_min:
        sizes = 20 + 180 * (score - s_min) / (s_max - s_min)
    else:
        sizes = np.full_like(score, 60, dtype=float)

    sc = ax.scatter(dens, rar, s=sizes, c=slack, cmap="coolwarm_r",
                    alpha=0.75, linewidths=0.5, edgecolors=BG)

    x_lo, x_hi = float(dens.min()), float(dens.max())
    y_lo, y_hi = float(rar.min()),  float(rar.max())
    x_pad = 0.05 * (x_hi - x_lo + 1e-9)
    y_pad = 0.08 * (y_hi - y_lo + 1e-9)
    ax.set_xlim(x_lo - x_pad, x_hi + x_pad)
    ax.set_ylim(max(0, y_lo - y_pad), y_hi + y_pad)

    # Iso-net-value diagonals: rarity = (density - net_value) / alpha
    if alpha > 0:
        xs = np.linspace(x_lo - x_pad, x_hi + x_pad, 50)
        # Pick 3 iso-lines at the 25th, 50th, 75th percentiles of net_value
        for pct, ls in [(25, ":"), (50, "--"), (75, ":")]:
            nv = np.percentile(net, pct)
            ys = (xs - nv) / alpha
            ax.plot(xs, ys, color=GRAY, linewidth=0.8,
                    linestyle=ls, alpha=0.5)
        
        nv_med = np.percentile(net, 50)
        # Find an x value where y is comfortably inside the y-range
        y_target = y_lo + 0.1 * (y_hi - y_lo)
        x_lbl = nv_med + alpha * y_target
        # Clamp into the x-range
        x_lbl = min(max(x_lbl, x_lo), x_hi - 0.2 * (x_hi - x_lo))
        ax.text(x_lbl, y_target, "median net value",
                color=GRAY, fontsize=7, fontstyle="italic",
                ha="left", va="bottom")

    ax.set_xlabel("Score density  (score / duration)")
    ax.set_ylabel("Project rarity cost  Σ (L / supply≥L)")
    style_ax(ax, "Reward vs Scarcity Tax   (size=score, color=slack)")
    cb = plt.colorbar(sc, ax=ax, fraction=0.04, pad=0.02)
    cb.ax.tick_params(labelcolor=WHITE, labelsize=7)
    cb.set_label("slack (days)", color=WHITE, fontsize=8)

    # Annotate top 3 by net_value (best projects under the new ranking)
    top_idx = np.argsort(-net)[:3]
    names = [m["projects"][i]["name"] for i in top_idx]
    ax.text(0.02, 0.98,
            f"α = {alpha:.3f}\nTop net-value:\n  " + "\n  ".join(names),
            transform=ax.transAxes, color=YELLOW,
            ha="left", va="top", fontsize=7,
            family="monospace")


def plot_staffability(ax, m):
    """
    Project staffing pressure = sum over roles of (global demand >= L) /
    (global supply >= L) for that role's skill. Each role contributes 1.0
    if its skill+level is perfectly tight (one role per qualified person),
    less if there's slack, more if it's contested.

    A project's "baseline" pressure (no contention anywhere) equals its
    number of roles; that's where the red dashed line sits. Projects
    above it are pulling on contested skills; way above = fighting for
    one of a handful of qualified contributors.
    """
    pressures = m["staffing_pressure"]
    if not pressures:
        style_ax(ax, "Project Staffing Pressure")
        return

    # The average number of roles per project. Pressure equal
    # to this means "average project where every skill+level is perfectly
    # supplied". Above it = contested.
    role_counts = [len(p["roles"]) for p in m["projects"]]
    avg_roles = float(np.mean(role_counts))

    contested = sum(1 for p, r in zip(pressures, role_counts) if p > r * 1.5)
    severe    = sum(1 for p, r in zip(pressures, role_counts) if p > r * 5)

    # Use log scale if the spread is wide
    p_min, p_max = min(pressures), max(pressures)
    use_log = p_max > 50 * max(p_min, 0.1)

    if use_log:
        clipped = [max(p, 0.1) for p in pressures]
        bins = np.logspace(np.log10(min(clipped)),
                           np.log10(max(clipped) * 1.01), 40)
        ax.hist(clipped, bins=bins, color=CYAN, edgecolor=BG, linewidth=0.4)
        ax.set_xscale("log")
    else:
        ax.hist(pressures, bins=40, color=CYAN, edgecolor=BG, linewidth=0.4)

    ax.axvline(avg_roles, color=MAGENTA, linewidth=1.5, linestyle="--",
               label=f"baseline ≈ {avg_roles:.1f} roles")
    ax.set_xlabel("Staffing pressure  Σ (demand≥L / supply≥L) per role")
    ax.set_ylabel("Number of projects")
    style_ax(ax, "Project Staffing Pressure  (baseline = #roles, no contention)")
    ax.legend(fontsize=7, facecolor=PANEL, labelcolor=WHITE, framealpha=0.7,
              loc="upper right")
    ax.text(0.98, 0.78,
            f"Contested (>1.5×): {contested}\nSevere (>5×): {severe}",
            transform=ax.transAxes, color=YELLOW,
            ha="right", va="top", fontsize=8)


def process_file(filepath, out_dir="visualizers"):
    C, P, contributors, projects = parse_input(filepath)
    m = compute_metrics(contributors, projects)

    title = filepath.split("/")[-1]
    stem  = title.rsplit(".", 1)[0]

    # Header stats
    n_skills      = len(m["all_skills"])
    n_roles       = sum(len(p["roles"]) for p in projects)
    total_score   = sum(p["score"] for p in projects)
    avg_slack     = float(np.mean(m["slacks"])) if m["slacks"] else 0.0
    max_lvl       = max(m["max_level_per_skill"].values()) if m["max_level_per_skill"] else 0

    fig = plt.figure(figsize=(18, 12), facecolor=BG)
    fig.suptitle(
        f"Hash Code 2022 — {title}",
        color=WHITE, fontsize=14, fontweight="bold", y=0.98
    )
    fig.text(
        0.5, 0.945,
        f"contributors = {C}  |  projects = {P}  |  roles = {n_roles}  "
        f"|  skills = {n_skills}  |  max level = {max_lvl}  "
        f"|  total possible score = {total_score:,}  |  mean slack = {avg_slack:.1f}d",
        color=GRAY, fontsize=9, ha="center", va="top", fontstyle="italic"
    )

    gs = gridspec.GridSpec(
        2, 3, figure=fig,
        hspace=0.45, wspace=0.40,
        left=0.06, right=0.97, top=0.88, bottom=0.07
    )
    axes = [fig.add_subplot(gs[i, j]) for i in range(2) for j in range(3)]
    ax1, ax2, ax3, ax4, ax5, ax6 = axes

    plot_supply_vs_demand(ax1, m, n=20)
    plot_scarcity_ranking(ax2, m, n=20)
    plot_level_heatmap(ax3, m, n=15)
    plot_slack_distribution(ax4, m)
    plot_density_vs_rarity(ax5, m)
    plot_staffability(ax6, m)

    import os
    os.makedirs(out_dir, exist_ok=True)
    out_path = f"{out_dir}/{stem}_dashboard.png"
    fig.savefig(out_path, dpi=150, bbox_inches="tight", facecolor=BG)
    plt.close(fig)
    return out_path


input_files = ["a_an_example.in", "b_better_start_small.in", "c_collaboration.in",
    "d_dense_schedule.in", "e_exceptional_skills.in", "f_find_great_mentors.in"]

if __name__ == "__main__":
    import sys, os
    files = sys.argv[1:] if len(sys.argv) > 1 else [
        f"input_files/{name}" for name in input_files
    ]
    for fp in files:
        if not os.path.exists(fp):
            print(f"skip (not found): {fp}")
            continue
        out = process_file(fp)
        print(f"wrote {out}")