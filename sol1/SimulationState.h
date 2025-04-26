#pragma once
#include <map>
#include <cmath>
#include <vector>
#include <bitset>

#include "Data.h"
#include "ProjectAllocator.h"

#define NMAX 999999999

using ProjectAllocation = std::pair<int, std::vector<std::string>>;

struct SimulationState
{
    const Data& data;
    int day, score_so_far;
    std::vector<ProjectAllocation> proj_to_contrib;
    std::map<std::string, int> available_at;
    std::map<std::string, std::set<std::string>[21]> skill_to_contributors;
    std::bitset<MAX_PROJECTS> project_done;

    explicit SimulationState(const Data& data, const std::map<std::string, std::set<std::string>[21]>& skill_to_contributors)
            : day(0)
            , score_so_far(0)
            , data(data)
            , skill_to_contributors(skill_to_contributors)
    {
        for (const auto& [skill, contribs_per_level] : skill_to_contributors)
        {
            for (int i = 0; i <= 20; ++i)
            {
                const auto& contr_per_levels = skill_to_contributors.find(skill)->second;
                if (contr_per_levels[i].empty())
                    continue;

                for (const auto& contrib_name : contr_per_levels[i])
                    available_at[contrib_name] = 0;
            }
        }
    }

    void add_allocation(const ProjectAllocation& allocation)
    {
        proj_to_contrib.push_back(allocation);
        const auto& [project_id, contributor_names] = allocation;
        project_done[project_id] = true;

        for (const auto& name : contributor_names)
        {
            available_at[name] = data.projects[project_id].length_in_days + day;
        }
    }

    void pass_days()
    {
        int day_to_jump_to = NMAX;
        for (const auto& [name, time] : available_at)
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

        return std::max(0, ((int)std::pow(project.score, 2) - manpower_penalty - day_penalty));
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
            const auto& contr_per_levels = skill_to_contributors.find(curr_skill)->second;
            bool ok = false;
            for (int i = level_req; i <= 20; ++i)
            {
                for (const auto& contrib_name : contr_per_levels[i])
                    if (available_at[contrib_name] <= day)
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
        std::set<std::string> contr_already_chosen;
        for (const auto& [curr_skill, level_req] : data.projects[project_index].skill_to_level)
        {
            const auto& contr_per_levels = skill_to_contributors.find(curr_skill)->second;
            bool ok = false;

            for (int i = level_req - 1; i <= 20; ++i)
            {
                for (const auto& contrib_name : contr_per_levels[i])
                    if (available_at[contrib_name] <= day && !contr_already_chosen.contains(contrib_name))
                    {
                        ok = true;
                        contr_already_chosen.insert(contrib_name);
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