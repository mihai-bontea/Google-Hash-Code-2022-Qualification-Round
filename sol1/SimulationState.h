#pragma once
#include <cmath>
#include <vector>
#include <bitset>

#include "SkillToContributorsMap.h"
#include "ProjectAllocator.h"

#define NMAX 999999999

class SimulationState
{
public:
    const Data& data;
    int day, score_so_far;
    std::vector<ProjectAllocation> proj_to_contrib;
    std::vector<int> available_at;
    SkillToContributorsMap skill_map;

    std::bitset<MAX_PROJECTS> project_done;

    explicit SimulationState(const Data& data, const std::vector<ContributorsPerSkill>& skill_to_contributors)
            : day(0)
            , score_so_far(0)
            , data(data)
            , skill_map(data)
    {
        available_at.resize(data.nr_contributors);
        for (const auto& contributors : skill_to_contributors)
        {
            for (int i = 0; i <= 20; ++i)
            {
                if (contributors[i].empty())
                    continue;

                for (const auto contrib_id : contributors[i])
                    available_at[contrib_id] = 0;
            }
        }
    }

    void add_allocation(const ProjectAllocation& allocation)
    {
        proj_to_contrib.push_back(allocation);
        const auto& [project_id, contributor_names] = allocation;
        project_done[project_id] = true;

        for (const auto contrib_id : contributor_names)
        {
            available_at[contrib_id] = data.projects[project_id].length_in_days + day;
        }
    }

    void pass_days()
    {
        int day_to_jump_to = NMAX;
        for (const int time : available_at)
        {
            if (time > day && time < day_to_jump_to)
                day_to_jump_to = time;
        }
        day = day_to_jump_to;
    }

    int heuristic_score(int project_index)
    {
        const auto& project = data.projects[project_index];
        int manpower_penalty = project.length_in_days * project.skill_to_level.size();
        int day_penalty = (project.best_before_day > day + project.length_in_days)? 0 :
                      day + project.length_in_days - project.best_before_day;
        int ahead_of_time_penalty = std::max(0, project.best_before_day - project.length_in_days - day) * 3;

        return std::max(0, ((int)std::pow(project.score, 2) - manpower_penalty - day_penalty - ahead_of_time_penalty));
    }

    int actual_score(int project_index)
    {
        const auto& project = data.projects[project_index];
        int penalty = (project.best_before_day > day + project.length_in_days)? 0 :
                day + project.length_in_days - project.best_before_day;
        return std::max(0, (project.score - penalty));
    };

    [[nodiscard]] bool can_project_be_done(int project_index) const
    {
        // Project has already been done/deemed unuseful
        if (project_done[project_index])
            return false;

        if (!is_hard_skill_requirement_met(project_index))
            return false;

        return is_unique_contributor_requirement_met(project_index);
    }

    // At least one non-unique* contributor who has that skill at >= level_req must be available
    // (The same contributor can mentor multiple roles)
    [[nodiscard]] bool is_hard_skill_requirement_met (int project_index) const
    {
        for (const auto &[skill_id, level_req]: data.projects[project_index].skill_to_level)
        {
            bool skill_can_be_staffed = false;

            const auto& has_skill_at_level = skill_map.at_least(skill_id, level_req);
            for (int contrib_id = 0; contrib_id < data.nr_contributors; ++contrib_id)
                if (has_skill_at_level[contrib_id] && available_at[contrib_id] <= day)
                {
                    skill_can_be_staffed = true;
                    break;
                }
            if (!skill_can_be_staffed)
                return false;
        }
        return true;
    }

    [[nodiscard]] bool is_unique_contributor_requirement_met(int project_index) const
    {
        // At least one unique contributor who has that skill at >= level_req - 1 must be available
        uset<int> contrib_already_chosen;

        for (const auto &[skill_id, level_req]: data.projects[project_index].skill_to_level)
        {
            // Everyone can work on level 1 with a mentor
            if (level_req == 1)
                continue;

            bool skill_can_be_staffed = false;

            const auto& has_skill_at_level = skill_map.at_least(skill_id, level_req);
            for (int contrib_id = 0; contrib_id < data.nr_contributors; ++contrib_id)
                if (has_skill_at_level[contrib_id] && available_at[contrib_id] <= day && !contrib_already_chosen.contains(contrib_id))
                {
                    skill_can_be_staffed = true;
                    contrib_already_chosen.insert(contrib_id);
                    break;
                }

            if (!skill_can_be_staffed)
                return false;
        }
        return true;
    }
};