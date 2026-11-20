#pragma once
#include <omp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "Data.h"
#include "SimulationState.h"

struct Solution
{
    long long score = 0;
    std::vector<ProjectAllocation> allocations; // in the order projects were started
};

// Everything that differs between two simulations.
struct Strategy
{
    bool dynamic_priority; // re-rank the waiting projects on every event day
    bool training;         // also start 0-point projects that level somebody up

    // Static priority (computed once):
    //   score^a / (length^b * roles^c * (best_before + 1)^d)
    double score_exp, length_exp, roles_exp, deadline_exp;

    // Dynamic priority (recomputed every event day), the original heuristic with tunable weights:
    //   score^a - w1 * length * roles - w2 * days_late - w3 * slack
    // where slack = days left before the latest on-time start
    double dyn_score_exp, manpower_weight, lateness_weight, slack_weight;

    // Each project's score term gets a fixed random factor in [1 - n, 1 + n]
    double noise_amplitude;

    static Strategy random(std::mt19937& rng)
    {
        std::uniform_real_distribution<double> u(0.0, 1.0);
        std::bernoulli_distribution coin(0.5);
        return {
                .dynamic_priority = coin(rng),
                .training         = coin(rng),
                .score_exp        = 0.5 + u(rng),
                .length_exp       = 0.5 + u(rng),
                .roles_exp        = u(rng),
                .deadline_exp     = 3.0 * u(rng),
                .dyn_score_exp    = 1.0 + 1.5 * u(rng),
                .manpower_weight  = 2.0 * u(rng),
                .lateness_weight  = 3.0 * u(rng),
                .slack_weight     = 6.0 * u(rng),
                .noise_amplitude  = 0.1 * u(rng),
        };
    }

    // The heuristic from the old backtracking version: score^2 - length*roles - days_late - 3*slack
    static Strategy old_heuristic()
    {
        Strategy s = {};
        s.dynamic_priority = true;
        s.training         = true;
        s.dyn_score_exp    = 2.0;
        s.manpower_weight  = 1.0;
        s.lateness_weight  = 1.0;
        s.slack_weight     = 3.0;
        s.noise_amplitude  = 0.0;
        return s;
    }
};

// One full greedy run: walk through time event by event, and on every event day
// start every project (in priority order) that can be fully staffed right now.
// All state lives in SimulationState; this class only makes the decisions.
class GreedyAllocator
{
public:
    GreedyAllocator(const Data& data, const SkillToContributorsMap& initial_skills,
                    const Strategy& strategy, std::mt19937& rng)
            : data(data)
            , strategy(strategy)
            , state(data, initial_skills)
            , priority(data.projects.size())
    {
        std::uniform_real_distribution<double> noise(1.0 - strategy.noise_amplitude, 1.0 + strategy.noise_amplitude);
        project_noise.resize(data.projects.size());
        for (double& n : project_noise)
            n = noise(rng);

        waiting_projects.resize(data.projects.size());
        std::iota(waiting_projects.begin(), waiting_projects.end(), 0);

        if (!strategy.dynamic_priority)
            sort_waiting_projects(&GreedyAllocator::static_priority);
    }

    Solution run()
    {
        while (true)
        {
            start_projects_for_today();

            // Nobody is busy, so nothing will change anymore
            if (!state.anyone_busy())
                break;

            state.pass_days();
        }
        return { state.score_so_far, std::move(state.proj_to_contrib) };
    }

private:
    static constexpr int NONE = -1;

    const Data& data;
    const Strategy strategy;
    SimulationState state;

    std::vector<int> waiting_projects; // not started yet, in priority order
    std::vector<double> project_noise;  // fixed per project for this simulation
    std::vector<double> priority;       // scratch buffer for sorting, indexed by project id

    [[nodiscard]] double static_priority(int project_id) const
    {
        const Project& project = data.projects[project_id];
        return std::pow(project.score, strategy.score_exp) * project_noise[project_id]
               / (std::pow(project.length_in_days, strategy.length_exp)
                  * std::pow(project.skill_to_level.size(), strategy.roles_exp)
                  * std::pow(project.best_before_day + 1, strategy.deadline_exp));
    }

    [[nodiscard]] double dynamic_priority(int project_id) const
    {
        const Project& project = data.projects[project_id];
        const double manpower = double(project.length_in_days) * project.skill_to_level.size();
        const double slack = std::max(0, project.best_before_day - project.length_in_days - state.day);

        return std::pow(project.score, strategy.dyn_score_exp) * project_noise[project_id]
               - strategy.manpower_weight * manpower
               - strategy.lateness_weight * state.days_late_if_started_today(project_id)
               - strategy.slack_weight * slack;
    }

    void sort_waiting_projects(double (GreedyAllocator::*priority_of)(int) const)
    {
        for (const int p : waiting_projects)
            priority[p] = (this->*priority_of)(p);

        std::ranges::sort(waiting_projects, std::ranges::greater{}, [&](int p) { return priority[p]; });
    }

    void start_projects_for_today()
    {
        if (strategy.dynamic_priority)
            sort_waiting_projects(&GreedyAllocator::dynamic_priority);

        start_paying_projects();

        if (strategy.training)
            start_training_projects();
    }

    // Pass 1: projects that still give points, in priority order
    void start_paying_projects()
    {
        std::vector<int> still_waiting;
        still_waiting.reserve(waiting_projects.size());

        for (const int project_id : waiting_projects)
        {
            // Points only go down over time, so a worthless project stays worthless.
            // Keep it only if it may still be used for training.
            if (state.actual_score(project_id) == 0)
            {
                if (strategy.training)
                    still_waiting.push_back(project_id);
                continue;
            }

            if (!try_start(project_id, /*only_if_someone_learns=*/false))
                still_waiting.push_back(project_id);
        }

        waiting_projects = std::move(still_waiting);
    }

    // Pass 2: with whoever is still free, 0-point projects that level somebody up
    void start_training_projects()
    {
        std::vector<int> still_waiting;
        still_waiting.reserve(waiting_projects.size());

        for (const int project_id : waiting_projects)
        {
            const bool worthless = state.actual_score(project_id) == 0;
            if (!worthless || !try_start(project_id, /*only_if_someone_learns=*/true))
                still_waiting.push_back(project_id);
        }

        waiting_projects = std::move(still_waiting);
    }

    // Staffs and starts the project if possible. Returns true if it was started.
    bool try_start(int project_id, bool only_if_someone_learns)
    {
        if (!state.can_project_be_done(project_id))
            return false;

        auto team = try_staff(data.projects[project_id]);
        if (!team)
            return false;

        if (only_if_someone_learns && state.count_learners(project_id, *team) == 0)
            return false;

        state.add_allocation(project_id, std::move(*team));
        return true;
    }

    // Returns one contributor per role (in the project's role order), or nullopt.
    [[nodiscard]] std::optional<std::vector<int>> try_staff(const Project& project) const
    {
        const auto& roles = project.skill_to_level;

        // Hardest roles first: high-level people get picked early and can then mentor the rest
        std::vector<int> role_order(roles.size());
        std::iota(role_order.begin(), role_order.end(), 0);
        std::ranges::stable_sort(role_order, std::ranges::greater{}, [&](int r) { return roles[r].second; });

        std::vector<int> team(roles.size(), NONE);
        ContributorBits chosen;

        for (std::size_t i = 0; i < role_order.size(); ++i)
        {
            const int role = role_order[i];
            const auto [skill, level] = roles[role];

            const std::span<const int> later_roles(role_order.begin() + i + 1, role_order.end());
            const auto mentoring_needs = unmet_mentoring_needs(roles, later_roles, chosen);

            const int c = pick_contributor(skill, level, chosen, mentoring_needs);
            if (c == NONE)
                return std::nullopt;

            team[role] = c;
            chosen[c] = true;
        }
        return team;
    }

    // For each role still to be staffed that nobody on the team can mentor yet:
    // the set of people who could mentor it (level >= the role's level in its skill).
    [[nodiscard]] std::vector<const ContributorBits*> unmet_mentoring_needs(
            const std::vector<std::pair<int, int>>& roles,
            std::span<const int> later_roles,
            const ContributorBits& chosen) const
    {
        std::vector<const ContributorBits*> needs;
        for (const int role : later_roles)
        {
            const auto [skill, level] = roles[role];
            const ContributorBits& possible_mentors = state.skill_map.at_least(skill, level);
            if ((chosen & possible_mentors).none())
                needs.push_back(&possible_mentors);
        }
        return needs;
    }

    // Candidate scoring
    //
    // value = fit + MENTOR_BONUS * (later roles nobody can mentor yet that they could)
    // fit   = LEARN_BONUS if they level up from this role
    //       + MENTEE_BONUS if they're below the requirement (a qualified person stays free)
    //       - OVERQUALIFIED_PENALTY per level above the requirement
    static constexpr double LEARN_BONUS           = 1.0;
    static constexpr double MENTEE_BONUS          = 1.0;
    static constexpr double MENTOR_BONUS          = 0.8;
    static constexpr double OVERQUALIFIED_PENALTY = 0.6;

    // Value of putting someone at `candidate_level` on a role needing `role_level`,
    // before counting what else they could mentor. Never increases with candidate_level.
    [[nodiscard]] static double fit_value(int candidate_level, int role_level, int max_level)
    {
        double value = 0.0;
        if (candidate_level <= role_level && candidate_level < max_level)
            value += LEARN_BONUS;
        if (candidate_level < role_level)
            value += MENTEE_BONUS;
        value -= OVERQUALIFIED_PENALTY * std::max(0, candidate_level - role_level);
        return value;
    }

    // The highest-value free contributor for one role, or NONE.
    // Candidates are grouped by level: everyone at one level has the same fit, so within
    // a level only mentoring coverage decides.
    [[nodiscard]] int pick_contributor(int skill, int level, const ContributorBits& chosen,
                                       const std::vector<const ContributorBits*>& mentoring_needs) const
    {
        const auto& skills = state.skill_map;
        const ContributorBits available = state.free_now & ~chosen;

        // Someone one level below can take the role only if the team already has a mentor
        const bool mentor_in_team = (chosen & skills.at_least(skill, level)).any();
        const int lowest_level = mentor_in_team ? level - 1 : level;
        const double max_mentor_value = MENTOR_BONUS * static_cast<double>(mentoring_needs.size());

        int best = NONE;
        double best_value = -std::numeric_limits<double>::infinity();

        for (int l = lowest_level; l <= skills.max_level(skill); ++l)
        {
            const double fit = fit_value(l, level, skills.max_level(skill));

            // Fit only gets worse at higher levels: nobody from here on can win
            if (fit + max_mentor_value <= best_value)
                break;

            const auto [c, covered] = best_mentor_among(available & skills.exactly(skill, l), mentoring_needs);
            if (c == NONE)
                continue;

            const double value = fit + MENTOR_BONUS * covered;
            if (value > best_value)
            {
                best = c;
                best_value = value;
            }
        }
        return best;
    }

    struct MentorCandidate
    {
        int contributor; // NONE if there were no candidates
        int covered;     // how many of the mentoring needs they could cover
    };

    // The candidate who could mentor the most of `mentoring_needs` (ties: lowest id).
    [[nodiscard]] static MentorCandidate best_mentor_among(const ContributorBits& candidates,
                                                           const std::vector<const ContributorBits*>& mentoring_needs)
    {
        if (mentoring_needs.empty())
            return { first_of(candidates), 0 };

        MentorCandidate best = { NONE, -1 };
        for (std::size_t c = candidates._Find_first(); c < candidates.size(); c = candidates._Find_next(c))
        {
            int covered = 0;
            for (const ContributorBits* possible_mentors : mentoring_needs)
                covered += (*possible_mentors)[c];

            if (covered > best.covered)
            {
                best = { static_cast<int>(c), covered };

                // Can't do better than covering every need
                if (covered == static_cast<int>(mentoring_needs.size()))
                    break;
            }
        }
        return best;
    }

    [[nodiscard]] static int first_of(const ContributorBits& bits)
    {
        const std::size_t i = bits._Find_first();
        return i < bits.size() ? static_cast<int>(i) : NONE;
    }
};

// Run independent greedy simulations on every thread and return the best solution found.
// Stops when the time budget runs out, or earlier if no thread has improved in a while (`patience`)
inline Solution solve_in_parallel(const Data& data,
                                  std::chrono::seconds time_budget,
                                  std::chrono::seconds patience = std::chrono::seconds(60))
{
    using clock = std::chrono::steady_clock;

    const SkillToContributorsMap initial_skills(data);
    const auto start = clock::now();
    const auto deadline = start + time_budget;
    const auto base_seed = static_cast<unsigned>(start.time_since_epoch().count());

    // Shared progress tracking, so every thread stops when the search as a whole stalls
    long long best_score_so_far = 0;
    std::atomic<clock::rep> last_improvement{ start.time_since_epoch().count() };

    auto should_stop = [&] {
        const auto now = clock::now();
        const auto since_improvement = now - clock::time_point(clock::duration(last_improvement.load()));
        return now >= deadline || since_improvement >= patience;
    };

    Solution best;
    long long simulations_run = 0;

    #pragma omp parallel reduction(+ : simulations_run)
    {
        const int thread_id = omp_get_thread_num();
        std::seed_seq seed{ base_seed, static_cast<unsigned>(thread_id) };
        std::mt19937 rng(seed);
        Solution thread_best;

        // Thread 0 starts with the old heuristic
        bool first_run = true;

        while (!should_stop())
        {
            const Strategy strategy = (thread_id == 0 && first_run) ? Strategy::old_heuristic()
                                                                    : Strategy::random(rng);
            first_run = false;

            Solution candidate = GreedyAllocator(data, initial_skills, strategy, rng).run();
            ++simulations_run;

            if (candidate.score <= thread_best.score)
                continue;

            // Improvements are rare, so a critical section here costs nothing
            #pragma omp critical(progress)
            if (candidate.score > best_score_so_far)
            {
                best_score_so_far = candidate.score;
                last_improvement = clock::now().time_since_epoch().count();
                std::cout << "Score " << candidate.score << " after "
                          << std::chrono::duration_cast<std::chrono::seconds>(clock::now() - start).count() << "s"
                          << (strategy.dynamic_priority ? " [dynamic" : " [static")
                          << (strategy.training ? ", training]\n" : "]\n");
            }

            thread_best = std::move(candidate);
        }

        #pragma omp critical
        if (thread_best.score > best.score)
            best = std::move(thread_best);
    }

    std::cout << "Simulations: " << simulations_run << ", best score: " << best.score << '\n';
    return best;
}