# Google Hash Code 2022 Qualification Round

>You are given a list of contributors, who have already mastered various skills, and a list of projects with different skill requirements. Contributors can improve their skills by completing projects and can mentor each other to work is roles in which they couldn't succeed on their own. Your task is to assign contributors to project roles that fit their qualifications and maximize the score for completed projects.

## Solution 1:

### Strategy

At each step, a priority queue with the best scoring project is populated(also taking the overdue project penalty into account). A quick check is done on whether the project could be done with the contributors available at that specific day(for each role, at least one non-unique contributor who has the skill at the required level or higher, and at least one unique contributor who has the skill at level - 1). This method never leads to false negatives, but can lead to false positives. This is ok because it only needs to be good enough to skip the vast majority of backtracking operations while also having very little overhead.

Then, **using a thread pool, 10 backtracking tasks are simultaneously started** on a randomly shuffled array representing the project roles. Each has a 25 second timer after which execution is stopped even if no solution has been found. Out of these 10 solutions, **the one that leads to the most mentorship/learning is selected**. If no solution can be found at that moment, the simulation day is increased.

By analyzing the input files, we can see that the highest skill level required for a project is 20. This means that for each (skill, level) pair, we can greatly reduce the lookup time for contributors having that skill at that level or higher, by splitting the set of contributors into an array of 21 sets, such that set[i] contains the contributors who have that skill at exactly level i. This might seem small but makes a great difference in speed.

### Scoring

| Input File              | Score      | Skill Increase |
|-------------------------|------------|----------------|
| a_an_example            | 20         | 0              |
| b_better_start_small    | 323,526    | 20             |
| c_collaboration         | 249,602    | 0              |
| d_dense_schedule        | 53,712     | 8              |
| e_exceptional_skills    | 1,591,807  | 619            |
| f_find_great_mentors    | 592,707    | 52,166         |
| **Total**               | **2,811,374** | **52,813**  |

Thread pool used: https://github.com/bshoshany/thread-pool