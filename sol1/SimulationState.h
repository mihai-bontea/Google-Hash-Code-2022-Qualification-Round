#pragma once
#include <algorithm>
#include <cassert>
#include <functional>
#include <utility>
#include <vector>

#include "Data.h"
#include "SkillToContributorsMap.h"

class SimulationState
{
public:
    const Data& data;
    int day = 0;
    long long score_so_far = 0;
    std::vector<ProjectAllocation> proj_to_contrib; // in the order projects were started
    ContributorBits free_now;                       // contributors not working on anything today
    SkillToContributorsMap skill_map;

    SimulationState(const Data& data, const SkillToContributorsMap& initial_skills)
            : data(data)
            , skill_map(initial_skills)
    {
        for (int c = 0; c < data.nr_contributors; ++c)
            free_now[c] = true;
    }

    [[nodiscard]] bool anyone_busy() const { return !releases.empty(); }

    // Jump to the next day someone finishes a project, and free everyone finishing then.
    void pass_days()
    {
        assert(anyone_busy());
        day = releases.front().first;

        while (!releases.empty() && releases.front().first <= day)
        {
            free_now[releases.front().second] = true;
            std::ranges::pop_heap(releases, std::greater<>{});
            releases.pop_back();
        }
    }

    [[nodiscard]] int days_late_if_started_today(int project_index) const
    {
        const Project& project = data.projects[project_index];
        return std::max(0, day + project.length_in_days - project.best_before_day);
    }

    [[nodiscard]] int actual_score(int project_index) const
    {
        return std::max(0, data.projects[project_index].score - days_late_if_started_today(project_index));
    }

    // Cheap necessary conditions; a full team may still not exist.
    [[nodiscard]] bool can_project_be_done(int project_index) const
    {
        const auto& roles = data.projects[project_index].skill_to_level;
        if (free_now.count() < roles.size())
            return false;

        return is_hard_skill_requirement_met(project_index);
    }

    // Every role needs someone free at >= its level in its skill:
    // either to fill it, or to mentor whoever fills it.
    [[nodiscard]] bool is_hard_skill_requirement_met(int project_index) const
    {
        for (const auto& [skill_id, level_req] : data.projects[project_index].skill_to_level)
            if ((skill_map.at_least(skill_id, level_req) & free_now).none())
                return false;
        return true;
    }

    // How many team members would gain a level that matters from this project:
    // level <= role level, and not already at the highest level any project needs.
    [[nodiscard]] int count_learners(int project_index, const std::vector<int>& team) const
    {
        const auto& roles = data.projects[project_index].skill_to_level;

        int learners = 0;
        for (std::size_t role = 0; role < team.size(); ++role)
        {
            const auto& [skill_id, level_req] = roles[role];
            const int current = skill_map.level(team[role], skill_id);
            if (current <= level_req && current < skill_map.max_level(skill_id))
                ++learners;
        }
        return learners;
    }

    // Start the project today with 'team' (one contributor per role, in role order):
    // adds its score, levels up whoever learns, and marks the team busy until it ends.
    void add_allocation(int project_index, std::vector<int> team)
    {
        const Project& project = data.projects[project_index];
        assert(team.size() == project.skill_to_level.size());

        score_so_far += actual_score(project_index);
        const int end_day = day + project.length_in_days;

        for (std::size_t role = 0; role < team.size(); ++role)
        {
            const auto& [skill_id, level_req] = project.skill_to_level[role];
            const int c = team[role];

            // Applied now instead of at end_day: c is busy until then anyway
            skill_map.level_up_if_learning(c, skill_id, level_req);

            free_now[c] = false;
            releases.emplace_back(end_day, c);
            std::ranges::push_heap(releases, std::greater<>{});
        }

        proj_to_contrib.emplace_back(project_index, std::move(team));
    }

private:
    using Release = std::pair<int, int>; // (day the contributor is free again, contributor id)
    std::vector<Release> releases;       // min-heap on day
};