#pragma once

#include <chrono>

#include "SimulationState.h"

using namespace std::chrono;

class ProjectAllocator
{
public:
    static std::vector<int> find_allocation_for_project(SimulationState& simulation_state, const Project& project)
    {
        std::vector<int> current_solution_alloc, valid_solution_alloc;
        std::map<int, bool> contributor_already_chosen;

        auto start = steady_clock::now();
        std::function<void(int)> find_allocation_backtracking = [&](int step){

            auto now = steady_clock::now();
            auto elapsed = duration_cast<seconds>(now - start);
            if (!valid_solution_alloc.empty() || elapsed.count() >= 20)
                return;

            if (step == project.skill_to_level.size())
            {
                int skill_id = 0;
                bool solution_is_valid = true;
                for (int contributor_id : current_solution_alloc)
                {
                    auto [curr_skill, level_req] = project.skill_to_level[skill_id];
                    if (simulation_state.contributors[contributor_id].skill_to_level[curr_skill] < level_req)
                    {
                        auto can_others_mentor = [&](){
                             for (int colleague_id : current_solution_alloc)
                                 if (colleague_id != contributor_id)
                                 {
                                     auto colleague_skill_level = simulation_state.contributors[colleague_id].skill_to_level[curr_skill];
                                     if (colleague_skill_level >= level_req)
                                         return true;
                                 }
                             return false;
                        };
                        if (!can_others_mentor())
                        {
                            solution_is_valid = false;
                            break;
                        }
                    }
                    skill_id++;
                }
                if (solution_is_valid)
                    valid_solution_alloc = current_solution_alloc;
            }
            else
            {
                auto [curr_skill, level_req] = project.skill_to_level[step];

                // Contributors who have this skill
                for (int contributor_id : simulation_state.skill_to_contrib_id[curr_skill])
                {
                    auto& contributor = simulation_state.contributors[contributor_id];
                    const int contributor_skill = contributor.skill_to_level[curr_skill];

                    // Too low skill, already chosen for this project, or busy with another project
                    if (contributor_skill < level_req - 1 ||
                    contributor_already_chosen[contributor_id] ||
                    contributor.busy_until > simulation_state.day)
                        continue;

                    contributor_already_chosen[contributor_id] = true;
                    current_solution_alloc.push_back(contributor_id);

                    find_allocation_backtracking(step + 1);

                    current_solution_alloc.pop_back();
                    contributor_already_chosen[contributor_id] = false;
                }
            }
        };

        find_allocation_backtracking(0);
        return valid_solution_alloc;
    }

    static void update_contributors(const Data& data, SimulationState& simulation_state, const ProjectAllocation& project_allocation)
    {
        const auto& [project_id, contributor_ids] = project_allocation;
        const auto& project = data.projects[project_id];

        int skill_id = 0;
        for (int contributor_id : contributor_ids)
        {
            auto& contributor = simulation_state.contributors[contributor_id];
            contributor.busy_until = simulation_state.day + project.length_in_days;

            auto [curr_skill, level_req] = project.skill_to_level[skill_id];
            if (contributor.skill_to_level[curr_skill] < level_req)
                contributor.skill_to_level[curr_skill] = level_req;
            skill_id++;
        }
    }
};