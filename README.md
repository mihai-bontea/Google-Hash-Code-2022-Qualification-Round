# Google Hash Code 2022 Qualification Round

>You are given a list of contributors, who have already mastered various skills, and a list of projects with different skill requirements. Contributors can improve their skills by completing projects and can mentor each other to work is roles in which they couldn't succeed on their own. Your task is to assign contributors to project roles that fit their qualifications and maximize the score for completed projects.

## Solution 1:

### Strategy

Instead of one carefully searched simulation, **many independent greedy simulations run in parallel with OpenMP**, each thread with its own RNG, and the best result is kept. Each simulation gets a random strategy: either a static project order (score, length, role count and deadline with random exponents) or a dynamic one recomputed every day from the original heuristic with random weights (score² minus manpower, overdue and ahead-of-time penalties). Simulations jump straight from one day when contributors finish to the next. Optionally, a second pass starts 0-point projects with leftover contributors, as long as somebody gains a level that is still needed. The search stops after 15 minutes, or earlier if the best score hasn't improved for a set time.

Before staffing, a quick bitset check rejects most projects that can't be done: for each role, someone free at the required level or higher must exist. The team is then built **greedily, hardest roles first**, so high-level people join early and can mentor the rest. Each candidate is scored with fixed weights: **a bonus for learning from the role, a bonus for being a mentee, a penalty per level of over-qualification, and a bonus for each later role they could mentor**. A good mentor can therefore be chosen over someone at the exact level, but only when they cover enough later roles to justify it.

### Data layout optimizations

For each skill, the contributors are stored as **cumulative bitsets**: row L holds everyone at level L or higher. Levels are capped at the highest level any project requires, and all rows sit in one flat array(This has smaller memory footprint than the previous array of sets, and has better cache locality). "At least L" is a single row, "exactly L" is two rows XOR-ed, and a level-up sets one bit. Combined with a bitset of free contributors, candidate searches become word-wise ANDs.

All strings appearing in the input (contributor, project and skill names) are translated into integers such that maps can be turned into plain vectors and sets can be turned into bitsets. This also avoids string operations inside the hot path. When writing the output, the ids are translated back to names.

### Scoring

| Input File              | Score       | Skill Increase |
|-------------------------|-------------|----------------|
| a_an_example            | 40          | 3              |
| b_better_start_small    | 971,561     | 110            |
| c_collaboration         | 306,550     | 3,088          |
| d_dense_schedule        | 255,694     | 499            |
| e_exceptional_skills    | 1,620,596   | 1,007          |
| f_find_great_mentors    | 653,951     | 67,100         |
| **Total**               | **3,808,392** | **71,807**   |

## Input file visualization

<img width="2714" height="1729" alt="Image" src="https://github.com/user-attachments/assets/6c49a553-ab64-4925-a6e8-543520471635" />

### 1) Top N Skills: Supply vs Demand

For the most-demanded skills:

- 🟩 **Green** = number of contributors with the skill at level `>= 1`
- 🟥 **Red** = total roles across all projects requiring it

---

### 2) Top N Scarcest Skills

A single rating that collapses supply and demand into one score per skill:

**scarcity(skill) = Σ_L  demand_at_L(skill) · L / max(1, supply_≥L(skill))**

Skills at the top are the leverage points: **staff them first and many projects unlock**.

---

### 3) Supply (>=L) - Demand (=L) Heatmap

The same metric as above, but decomposed by level. Helps show which levels are more undersupplied.

---

### 4) Project Slack Distribution

**slack = best before - duration**

- Negative slack means that the project is impossible to complete.
- Tight count (`< 5` days) and the median are also displayed.

---

### 5) Reward vs Scarcity Tax

The strategic tradeoff plot:

Each project is defined by:

- **Score density** (`score / duration`)
- **Project rarity cost** (does it use scarce skills?)

Additional encodings:

- **Point size** ∝ total project score
- **Color** = slack in days
  - 🔴 Red → tight deadline
  - 🔵 Blue → lots of room

---

### 6) Project Staffing Pressure

For each role, we are interested in the congestion: global demand / global supply, i.e. **how many other roles across all projects are competing for the same pool of people**.

The dashed line marks the average roles-per-project baseline. Projects above it are fighting for contested talent.

Unlike the rarity cost in the previous panel (which weighs by level), this metric weighs by competition. A project can be low rarity but high pressure (common skills, but everyone wants them) or high rarity but low pressure (deep expert needed, but only this one project needs them).