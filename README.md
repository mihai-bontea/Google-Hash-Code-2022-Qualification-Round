# Google Hash Code 2022 Qualification Round

>You are given a list of contributors, who have already mastered various skills, and a list of projects with different skill requirements. Contributors can improve their skills by completing projects and can mentor each other to work is roles in which they couldn't succeed on their own. Your task is to assign contributors to project roles that fit their qualifications and maximize the score for completed projects.

## Solution 1:

### Strategy

At each step, a priority queue with the best scoring project is populated(also taking the overdue project penalty into account). A quick check is done on whether the project could be done with the contributors available at that specific day(for each role, at least one non-unique contributor who has the skill at the required level or higher, and at least one unique contributor who has the skill at level - 1). This method never leads to false negatives, but can lead to false positives. This is ok because it only needs to be good enough to skip the vast majority of backtracking operations while also having very little overhead.

Then, **using a thread pool, 10 backtracking tasks are simultaneously started** on a randomly shuffled array representing the project roles. Each has a 15 second timer after which execution is stopped even if no solution has been found. Out of these 10 solutions, **the one that leads to the most mentorship/learning is selected**. If no solution can be found at that moment, the simulation day is increased.

By analyzing the input files, we can see that the highest skill level required for a project is 20. This means that for each (skill, level) pair, we can greatly reduce the lookup time for contributors having that skill at that level or higher, by splitting the set of contributors into an array of 21 sets, such that set[i] contains the contributors who have that skill at exactly level i. This might seem small but makes a great difference in speed.

### Scoring

| Input File              | Score      | Skill Increase |
|-------------------------|------------|----------------|
| a_an_example            | 20         | 0              |
| b_better_start_small    | 344,991    | 29             |
| c_collaboration         | 255,027    | 57             |
| d_dense_schedule        | 53,712     | 8              |
| e_exceptional_skills    | 1,601,565  | 1,464          |
| f_find_great_mentors    | 599,091    | 54,939         |
| **Total**               | **2,854,406** | **56,497**  |

## Input file visualization

<img width="2714" height="1729" alt="Image" src="https://github.com/user-attachments/assets/6c49a553-ab64-4925-a6e8-543520471635" />

### 1) Top N Skills: Supply vs Demand

For the most-demanded skills:

- 🟩 **Green** = number of contributors with the skill at level `>= 1`
- 🟥 **Red** = total roles across all projects requiring it

---

### 2) Top N Scarcest Skills

A single rating that collapses supply and demand into one score per skill:

\[
\text{scarcity(skill)} =
\sum_L
\frac{
\text{demand\_at\_L(skill)} \cdot L
}{
\max(1,\ \text{supply}_{\ge L}(\text{skill}))
}
\]

Skills at the top are the **leverage points**: staff them first and many projects unlock.

---

### 3) Supply (`>= L`) - Demand (`= L`) Heatmap

The same metric as above, but decomposed by level.

Helps show which levels are more undersupplied.

---

### 4) Project Slack Distribution

\[
\text{slack} = \text{best before} - \text{duration}
\]

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

For each role, we are interested in the **congestion**:

\[
\text{congestion} =
\frac{\text{global demand}}{\text{global supply}}
\]

i.e. how many other roles across all projects are competing for the same pool of people.

The dashed line marks the **average roles-per-project baseline**.

Projects above it are fighting for contested talent.

> Unlike the rarity cost in the previous panel (which weighs by level), this metric weighs by competition.

A project can be:

- **Low rarity but high pressure**  
  (common skills, but everyone wants them)

or

- **High rarity but low pressure**  
  (deep expert needed, but only this one project needs them)