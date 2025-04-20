#pragma once

#include <chrono>
#include <algorithm>
#include <random>

#include "SimulationState.h"
#include "BS_thread_pool.hpp"

using namespace std::chrono;

using WeightedAllocation = std::pair<int, std::vector<std::string>>;

class ProjectAllocator
{
public:
    static WeightedAllocation find_alloc(const SimulationState& simulation_state,
                                         const std::vector<std::pair<std::string, int>>& skill_to_level,
                                         bool skip_mentoring)
    {
        std::random_device rd;
        std::mt19937 g(rd());
        auto skill_to_level_copy = skill_to_level;
        std::shuffle(skill_to_level_copy.begin(), skill_to_level_copy.end(), g);

        std::set<std::string> contr_already_chosen;
        std::map<std::string, std::string> role_to_contr, valid_solution;
        int solution_points = 0;

        auto start = steady_clock::now();
        std::function<void(int)> find_allocation_backtracking = [&](int step){
            auto now = steady_clock::now();
            // Solution found/timer expired, stop execution
            auto elapsed = duration_cast<seconds>(now - start);
            if (!valid_solution.empty() || elapsed.count() >= 15)
            {
                return;
            }

            if (step == skill_to_level.size())
            {
                bool is_solution_valid = true;
                int learning_points = 0;
                for (const auto& [curr_skill, level_req] : skill_to_level)
                {
                    const auto& contr_for_role = role_to_contr[curr_skill];
                    const auto& contr_per_levels = simulation_state.skill_to_contributors.find(curr_skill)->second;

                    auto can_others_mentor = [&contr_per_levels, &role_to_contr, level_req](){
                        for (const auto& [_, contr_name]: role_to_contr)
                        {
                            for (int level = level_req; level <= 20; ++level)
                            {

                                if (contr_per_levels[level].contains(contr_name)) {
                                    return true;
                                }
                            }
                        }
                        return false;
                    };

                    auto needs_mentorship = [&contr_per_levels, level_req](const std::string& contr_name){
                        return contr_per_levels[level_req - 1].contains(contr_name);
                    };

                    if (needs_mentorship(contr_for_role))
                    {
                        if (can_others_mentor())
                        {
                            learning_points++;
                        }
                        else
                        {
                            is_solution_valid = false;
                            break;
                        }
                    }
                }
                if (is_solution_valid)
                {
                    solution_points = learning_points;
                    valid_solution = role_to_contr;
                }
            }
            else
            {
                auto [curr_skill, level_req] = skill_to_level_copy[step];
                // For skill between [level_req - 1, max)
                for (int contr_level = level_req - 1 + skip_mentoring; contr_level <= 20; ++contr_level)
                {
                    const auto& contr_per_levels = simulation_state.skill_to_contributors.find(curr_skill)->second;
                    // Going over all contributors with skill == contr_level
                    for (const auto &contr_name: contr_per_levels[contr_level])
                    {
                        int contr_available_at = (simulation_state.available_at.find(contr_name)->second);

                        if (contr_already_chosen.contains(contr_name) || contr_available_at > simulation_state.day)
                        {
                            continue;
                        }
                        contr_already_chosen.insert(contr_name);
                        role_to_contr[curr_skill] = contr_name;

                        if (valid_solution.empty())
                            find_allocation_backtracking(step + 1);

                        contr_already_chosen.erase(contr_name);
                    }
                }
            }
        };
        find_allocation_backtracking(0);

        // No solution found in a reasonable timeframe
        if (valid_solution.empty())
            return {0, {}};

        // Formatting the solution
        std::vector<std::string> solution_in_order;
        for (const auto& [skill_name, _] : skill_to_level)
        {
            solution_in_order.push_back(valid_solution[skill_name]);
        }
        return {solution_points, solution_in_order};
    }

    static WeightedAllocation find_allocation_for_project(const SimulationState& simulation_state, const Project& project)
    {
        std::vector<std::pair<std::string, int>> skill_to_level;
        for (const auto& [skill_name, level_req] : project.skill_to_level)
            skill_to_level.emplace_back(skill_name, level_req);

        BS::thread_pool pool(10);
        std::vector<std::future<WeightedAllocation>> futures;
        for (int th_index = 0; th_index < 10; ++th_index)
        {
            bool skip_mentoring = th_index % 2;
            auto future_for_task = pool.submit_task([&]() { return find_alloc(simulation_state, skill_to_level, skip_mentoring); });
            futures.push_back(std::move(future_for_task));
        }

        WeightedAllocation best_allocation = {0, {}};
        for (auto& future : futures)
        {
            auto weighted_allocation = future.get();
            const auto& [learning_points, allocation] = weighted_allocation;

            if (!allocation.empty() && learning_points >= best_allocation.first)
                best_allocation = weighted_allocation;
        }
        return best_allocation;
    }

    static void update_contributors(const Data& data, SimulationState& simulation_state, const ProjectAllocation& project_allocation)
    {
        const auto& [project_id, contributor_names] = project_allocation;
        const auto& project = data.projects[project_id];

        int role_id = 0;
        for (const auto& name : contributor_names)
        {
            const auto& [role_name, skill_req] = project.skill_to_level[role_id];
            const auto contr_per_levels = simulation_state.skill_to_contributors.find(role_name)->second;

            if (contr_per_levels[skill_req - 1].contains(name))
            {
                contr_per_levels[skill_req - 1].erase(name);
                contr_per_levels[skill_req].insert(name);
            }
            role_id++;
        }
    }
};