#pragma once
#include <cmath>
#include <vector>
#include <bitset>

#include "Data.h"
#include "ProjectAllocator.h"

#define NMAX 999999999

struct SimulationState
{
    const Data& data;
    int day, score_so_far;
    std::vector<ProjectAllocation> proj_to_contrib;
    std::vector<int> available_at;
    std::vector<ContributorsPerSkill> skill_to_contributors;


    std::bitset<MAX_PROJECTS> project_done;

    explicit SimulationState(const Data& data, const std::vector<ContributorsPerSkill>& skill_to_contributors)
            : day(0)
            , score_so_far(0)
            , data(data)
            , skill_to_contributors(skill_to_contributors)
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

    bool can_project_be_done(int project_index)
    {
        // Project has already been done/deemed unuseful
        if (project_done[project_index])
            return false;

        // At least one non-unique contributor who has that skill at >= level_req must be available
        for (const auto& [curr_skill, level_req] : data.projects[project_index].skill_to_level)
        {
            const auto& contr_per_levels = skill_to_contributors[curr_skill];
            bool ok = false;
            for (int i = level_req; i <= 20; ++i)
            {
                for (const auto contrib_id : contr_per_levels[i])
                    if (available_at[contrib_id] <= day)
                    {
                        ok = true;
                        break;
                    }
                if (ok)
                    break;
            }
            if (!ok)
                return false;
        }

        // At least one unique contributor who has that skill at >= level_req - 1 must be available
        uset<int> contrib_already_chosen;
        for (const auto& [curr_skill, level_req] : data.projects[project_index].skill_to_level)
        {
            const auto& contr_per_levels = skill_to_contributors[curr_skill];
            bool ok = false;

            for (int i = level_req - 1; i <= 20; ++i)
            {
                for (const auto contrib_id : contr_per_levels[i])
                    if (available_at[contrib_id] <= day && !contrib_already_chosen.contains(contrib_id))
                    {
                        ok = true;
                        contrib_already_chosen.insert(contrib_id);
                        break;
                    }
                if (ok)
                    break;
            }
            if (!ok)
                return false;
        }
        return true;
    }
};