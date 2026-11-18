#pragma once
#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <vector>

#include "Data.h"

constexpr std::size_t MAX_CONTRIBUTORS = 1500;
using ContributorBits = std::bitset<MAX_CONTRIBUTORS>;

/*
 * For every skill 's' and level 'l' in [1, max_req['s']], a bitset of contributors whose level in 's' is >= 'l'.
 * Levels are capped at max_req['s'] (the highest level any project needs for skill 's')
 * Level 0 is implicit: everyone has every skill at level >= 0.
 */

class SkillToContributorsMap
{
public:
    explicit SkillToContributorsMap(const Data& data)
    {
        // For each skill, compute the offset in 'rows' where its data begins
        const int nr_skills = static_cast<int>(data.max_skill_level_req.size());
        row_begin.assign(nr_skills + 1, 0);
        for (int s = 0; s < nr_skills; ++s)
            row_begin[s + 1] = row_begin[s] + data.max_skill_level_req[s];

        rows.resize(row_begin[nr_skills]);

        // Skills that only appear in projects have no entry in skill_to_contributors; their rows simply stay empty.
        const int known_skills = std::min(nr_skills, static_cast<int>(data.skill_to_contributors.size()));
        for (int s = 0; s < known_skills; ++s)
        {
            const auto& per_level = data.skill_to_contributors[s];
            for (int lvl = 1; lvl < static_cast<int>(per_level.size()); ++lvl)
            {
                const int capped = std::min(lvl, max_level(s));
                for (const int c : per_level[lvl])
                    for (int l = 1; l <= capped; ++l)
                        at_least(s, l)[c] = true;
            }
        }
    }

    // 1) Does contributor c have skill s at exactly level l?
    [[nodiscard]] bool has_exactly(int c, int s, int l) const
    {
        assert(l >= 0 && l <= max_level(s));
        if (!has_at_least(c, s, l))
            return false;
        return l == max_level(s) || !at_least(s, l + 1)[c];
    }

    // 2) Does contributor c have skill s at level >= l?
    [[nodiscard]] bool has_at_least(int contributor_id, int skill_id, int level) const
    {
        assert(level >= 0 && level <= max_level(skill_id));
        return level == 0 || at_least(skill_id, level)[contributor_id];
    }

    // 3) Raise contributor c's level in skill s by one (saturates at max_req[s]).
    void level_up(int c, int s)
    {
        const int cur = level(c, s);
        if (cur < max_level(s))
            at_least(s, cur + 1)[c] = true;
    }

    // 4) Can anyone in `team` mentor skill s for a role requiring level l?
    [[nodiscard]] bool can_mentor(const std::vector<int>& team, int s, int l) const
    {
        assert(l >= 1 && l <= max_level(s));
        const ContributorBits& row = at_least(s, l);
        return std::ranges::any_of(team, [&](int c) { return row[c]; });
    }

    [[nodiscard]] const ContributorBits& at_least(int skill_id, int level) const
    {
        return rows[row_begin[skill_id] + level - 1];
    }

private:
    std::vector<int> row_begin;        // size nr_skills + 1; never changes after construction
    std::vector<ContributorBits> rows; // all skills' level rows, back to back

    [[nodiscard]] int max_level(int s) const { return row_begin[s + 1] - row_begin[s]; }

    [[nodiscard]] ContributorBits& at_least(int skill_id, int level)
    {
        return rows[row_begin[skill_id] + level - 1];
    }

    // Current (capped) level: at most max_req[s] <= 20 bit tests.
    [[nodiscard]] int level(int c, int s) const
    {
        int l = 0;
        while (l < max_level(s) && at_least(s, l + 1)[c])
            ++l;
        return l;
    }
};