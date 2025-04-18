#pragma once
#include <map>
#include <vector>

#include "Data.h"

#define NMAX 999999999

using ProjectAllocation = std::pair<int, std::vector<int>>;

struct SimulationState
{
    int day, score_so_far;
    std::vector<Contributor> contributors;
    std::vector<ProjectAllocation> proj_to_contrib;
    std::map<std::string, std::vector<int>> skill_to_contrib_id;

    explicit SimulationState(const std::vector<Contributor>& contributors)
            : day(0)
            , score_so_far(0)
            , contributors(contributors)
    {
        for (int contributor_index = 0; contributor_index < contributors.size(); ++contributor_index)
        {
            for (const auto& [skill, _] : contributors[contributor_index].skill_to_level)
                skill_to_contrib_id[skill].push_back(contributor_index);
        }
    }

    void add_allocation(const ProjectAllocation& allocation)
    {
        proj_to_contrib.push_back(allocation);
    }

    void pass_days()
    {
        int day_to_jump_to = 999999999;
        for (const auto& contributor : contributors)
        {
            if (contributor.busy_until > day && contributor.busy_until < day_to_jump_to)
                day_to_jump_to = contributor.busy_until;
        }
        day = day_to_jump_to;
    }
};