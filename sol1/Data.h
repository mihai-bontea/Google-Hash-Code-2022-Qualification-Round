#pragma once
#include <algorithm>
#include <fstream>
#include <map>
#include <unordered_set>
#include <vector>

#define MAX_PROJECTS 19413
using ProjectAllocation = std::pair<int, std::vector<std::string>>;

struct Project
{
    std::string name;
    int length_in_days, score, best_before_day;
    std::vector<std::pair<std::string, int>> skill_to_level;

    Project(std::string&& name,
            int length_in_days,
            int score,
            int best_before_day,
            std::vector<std::pair<std::string, int>>&& skill_to_level)
            : name(std::move(name))
            , length_in_days(length_in_days)
            , score(score)
            , best_before_day(best_before_day)
            , skill_to_level(std::move(skill_to_level))
    {}
};

class Data
{
public:
    int nr_contributors, nr_projects;
    std::map<std::string, std::set<std::string>[21]> skill_to_contributors;
    std::vector<Project> projects;

    Data(const std::string& filename)
    {
        std::ifstream fin(filename);
        fin >> nr_contributors >> nr_projects;

        // avoid re-allocations and over-allocations
        projects.reserve(nr_projects);

        std::map<std::string, std::unordered_set<std::string>> contr_to_skills;

        // Read the contributor info
        for (int contr_index = 0; contr_index < nr_contributors; ++contr_index)
        {
            std::string contr_name;
            int nr_skills;
            fin >> contr_name >> nr_skills;

            std::unordered_set<std::string> skills;
            while (nr_skills--)
            {
                std::string skill_name;
                int skill_level;
                fin >> skill_name >> skill_level;
                skills.insert(skill_name);
                skill_to_contributors[skill_name][skill_level].insert(contr_name);
            }
            contr_to_skills[contr_name] = skills;
        }
        // Read the project info
        for (int proj_index = 0; proj_index < nr_projects; ++proj_index)
        {
            std::string name;
            int length_in_days, score, best_before_day, nr_roles;
            std::vector<std::pair<std::string, int>> skill_to_level;
            fin >> name >> length_in_days >> score >> best_before_day >> nr_roles;
            skill_to_level.reserve(nr_roles);

            while (nr_roles--)
            {
                std::string skill_name;
                int skill_level;
                fin >> skill_name >> skill_level;
                skill_to_level.emplace_back(skill_name, skill_level);
            }

            projects.emplace_back(std::move(name), length_in_days, score, best_before_day, std::move(skill_to_level));
        }

        // No skill in input means skill at level 0(can be improved with mentoring)
        for (auto& [contr_name, skills] : contr_to_skills)
            for (auto& [skill_name, contr_to_skills_per_level] : skill_to_contributors)
                if (!skills.contains(skill_name))
                {
                    auto& level0_set_for_skill = contr_to_skills_per_level[0];
                    level0_set_for_skill.insert(contr_name);
                }

        // Sort projects based on best before day
        std::sort(projects.begin(), projects.end(), [](const auto& lhs, const auto& rhs){
            return lhs.best_before_day < rhs.best_before_day;
        });
    }

    void write_to_file(const std::string& filename, const std::vector<ProjectAllocation>& allocation) const
    {
        std::ofstream fout(filename);
        fout << allocation.size() << '\n';
        for (const auto& [project_id, contributor_names] : allocation)
        {
            fout << projects[project_id].name << '\n';
            for (const auto& contributor_name : contributor_names)
                fout << contributor_name << " ";
            fout << '\n';
        }
    }
};