#pragma once

#include <omp.h>
#include <chrono>
#include <random>
#include <algorithm>
#include <functional>

#include "SimulationState.h"

using namespace std::chrono;

using SkillToLevel = std::pair<int, int>;
using WeightedAllocation = std::pair<int, std::vector<int>>;


class ProjectAllocator
{
public:
    static std::vector<std::pair<SkillToLevel, int>> get_shuffled_roles(const std::vector<SkillToLevel>& project_roles)
    {
        // Tie the roles to their original index so that we can reconstruct the solution later
        std::vector<std::pair<SkillToLevel, int>> roles_to_original_index;
        for (int index = 0; index < project_roles.size(); ++index)
            roles_to_original_index.emplace_back(project_roles[index], index);

        // Shuffle the array
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(roles_to_original_index.begin(), roles_to_original_index.end(), g);
        return roles_to_original_index;
    }

    static bool could_role_be_theoretically_mentored(const SimulationState& simulation_state,
                                                     std::vector<std::pair<SkillToLevel, int>>& roles_to_original_index,
                                                     const std::vector<int>& role_to_contr,
                                                     const SkillToLevel& role,
                                                     int current_step)
    {
        // Check if contributors chosen so far can mentor
        for (int step = 0; step < current_step; ++step)
        {
            // Do the colleagues selected so far have the role skill at the required level?
            const int colleague_id = role_to_contr[step];
            if (simulation_state.skill_map.has_at_least(colleague_id, role.first, role.second))
                return true;
        }

        // None of the colleagues selected so far are able to mentor this skill, check other available contributors
        // who could fulfill other roles in this project

        for (int step = current_step + 1; step < roles_to_original_index.size(); ++step)
        {
            const auto &[curr_skill, level_req] = roles_to_original_index[step].first;
            const auto &has_skill_at_level = simulation_state.skill_map.at_least(curr_skill, level_req);

            for (int contributor_id = 0; contributor_id < simulation_state.data.nr_contributors; ++contributor_id)
                if (has_skill_at_level[contributor_id] &&
                    simulation_state.available_at[contributor_id] <= simulation_state.day)
                {
                    // Could mentor the role skill
                    if (simulation_state.skill_map.has_at_least(contributor_id, role.first, role.second))
                        return true;
                }
        }
        return false;
    }

    static WeightedAllocation find_alloc(const SimulationState& simulation_state,
                                         const std::vector<SkillToLevel>& project_roles,
                                         bool skip_mentoring)
    {
        auto roles_to_original_index = get_shuffled_roles(project_roles);
        std::vector<bool> contr_already_chosen(simulation_state.data.nr_contributors, false);

        std::vector<int> role_to_contr(project_roles.size()), valid_solution(project_roles.size());
        int solution_points = 0;
        bool solution_found = false;

        auto start = steady_clock::now();
        auto is_timer_expired = [&start](){
            auto now = steady_clock::now();
            auto elapsed = duration_cast<seconds>(now - start);
            return elapsed.count() >= 15;
        };

        std::function<void(int)> find_allocation_backtracking = [&](int step){
            if (step == project_roles.size())
            {
                bool is_solution_valid = true;
                int learning_points = 0;

                for (int role_index = 0; role_index < project_roles.size(); ++role_index)
                {
//                    const auto& [curr_skill, level_req] = roles_to_original_index[role_index].first;
                    const int curr_skill = roles_to_original_index[role_index].first.first;
                    const int level_req = roles_to_original_index[role_index].first.second;
                    const auto& contr_for_role = role_to_contr[role_index];

                    auto can_others_mentor = [&simulation_state, &role_to_contr, curr_skill, level_req](){
                        for (const int contr_id : role_to_contr)
                            if (simulation_state.skill_map.has_at_least(contr_id, curr_skill, level_req))
                                return true;
                        return false;
                    };

                    auto needs_mentorship = [&simulation_state, curr_skill, level_req](int contr_id){
                        return (simulation_state.skill_map.has_exactly(contr_id, curr_skill, level_req - 1));
                    };

                    if (needs_mentorship(contr_for_role))
                    {
                        if (can_others_mentor())
                            learning_points++;
                        else
                        {
                            is_solution_valid = false;
                            break;
                        }
                    }
                    if (simulation_state.skill_map.has_exactly(contr_for_role, curr_skill, level_req))
                        learning_points++;
                }
                if (is_solution_valid)
                {
                    solution_points = learning_points;
                    valid_solution = role_to_contr;
                    solution_found = true;
                }
            }
            else
            {
                const auto& [curr_skill, level_req] = roles_to_original_index[step].first;
                const auto& has_skill_at_level = simulation_state.skill_map.at_least(curr_skill, level_req);

                for (int contributor_id = 0; contributor_id < simulation_state.data.nr_contributors; ++contributor_id)
                    if (has_skill_at_level[contributor_id]
                    || (simulation_state.skill_map.has_exactly(contributor_id, curr_skill, level_req - 1)
                    && !skip_mentoring
                    && could_role_be_theoretically_mentored(simulation_state,
                                                             roles_to_original_index,
                                                             role_to_contr,
                                                             roles_to_original_index[step].first,
                                                             step)))
                    {
                        if (is_timer_expired() || solution_found)
                            break;

                        // Skip already chosen/busy contributors
                        const int contr_available_at = (simulation_state.available_at[contributor_id]);
                        if (contr_already_chosen[contributor_id] || contr_available_at > simulation_state.day)
                            continue;

                        contr_already_chosen[contributor_id] = true;
                        role_to_contr[step] = contributor_id;

                        find_allocation_backtracking(step + 1);

                        contr_already_chosen[contributor_id] = false;

                        if (is_timer_expired() || solution_found)
                            break;
                    }
            }
        };
        find_allocation_backtracking(0);

        // No solution found in a reasonable timeframe
        if (!solution_found)
            return {0, {}};

        // Formatting the solution
        std::vector<int> solution_in_order(project_roles.size());
        for (int index_in_shuffled = 0; index_in_shuffled < project_roles.size(); ++index_in_shuffled)
        {
            const auto& [skill_to_level, original_index] = roles_to_original_index[index_in_shuffled];

            solution_in_order[original_index] = valid_solution[index_in_shuffled];
        }

        return {solution_points, solution_in_order};
    }

    static WeightedAllocation find_allocation_for_project(const SimulationState& simulation_state, const Project& project)
    {
        std::vector<std::pair<int, int>> skill_to_level;
        for (const auto& [skill_name, level_req] : project.skill_to_level)
            skill_to_level.emplace_back(skill_name, level_req);

        WeightedAllocation results[10];
        WeightedAllocation best_allocation = {0, {}};

//        omp_set_num_threads(10);
//        #pragma omp parallel for
        for (int th_index = 0; th_index < 2; ++th_index)
        {
            bool skip_mentoring = ((th_index % 2) == 0);
            results[th_index] = find_alloc(simulation_state, skill_to_level, skip_mentoring);
        }
        for (int th_index = 0; th_index < 2; ++th_index)
        {
            const auto& [learning_points, allocation] = results[th_index];
            if (!allocation.empty() && learning_points >= best_allocation.first)
                best_allocation = results[th_index];
        }

        return best_allocation;
    }

    static void update_contributors(const Data& data, SimulationState& state, const ProjectAllocation& allocation)
    {
        const auto& [project_id, contributor_ids] = allocation;
        const auto& roles = data.projects[project_id].skill_to_level;
        assert(contributor_ids.size() == roles.size());

        for (std::size_t role = 0; role < roles.size(); ++role)
        {
            const auto [skill_id, level_req] = roles[role];
            const int c = contributor_ids[role];

            // Level up if current level <= level_req
            if (state.skill_map.has_exactly(c, skill_id, level_req) || !state.skill_map.has_at_least(c, skill_id, level_req))
                state.skill_map.level_up(c, skill_id);
        }
    }
};