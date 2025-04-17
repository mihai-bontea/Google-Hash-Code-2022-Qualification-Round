#pragma once
#include <algorithm>
#include <fstream>
#include <map>
#include <vector>

struct Contributor
{
    std::string name;
    std::map<std::string, int> skill_to_level;

    Contributor(std::string&& name, std::map<std::string, int>&& skill_to_level)
    : name(std::move(name))
    , skill_to_level(std::move(skill_to_level))
    {}

    bool operator<(const Contributor& rhs) const
    {
        return name < rhs.name;
    }
};

struct Project
{
    std::string name;
    int length_in_days, score, best_before_day;
    std::map<std::string, int> skill_to_level;

    Project(std::string&& name,
            int length_in_days,
            int score,
            int best_before_day,
            std::map<std::string, int>&& skill_to_level)
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
    std::vector<Contributor> contributors;
    std::vector<Project> projects;

    Data(const std::string& filename)
    {
        std::ifstream fin(filename);
        fin >> nr_contributors >> nr_projects;

        // avoid re-allocations and over-allocations
        contributors.reserve(nr_contributors);
        projects.reserve(nr_projects);

        auto read_skills = [&fin](int count){
            std::map<std::string, int> skill_to_level;
            while (count--)
            {
                std::string skill_name;
                int level;
                fin >> skill_name >> level;
                skill_to_level[skill_name] = level;
            }
            return  skill_to_level;
        };

        // Read the contributor info
        for (int contr_index = 0; contr_index < nr_contributors; ++contr_index)
        {
            std::string name;
            int nr_skills;
            fin >> name >> nr_skills;

            auto skill_to_level = read_skills(nr_skills);
            contributors.emplace_back(std::move(name), std::move(skill_to_level));
        }
        // Read the project info
        for (int proj_index = 0; proj_index < nr_projects; ++proj_index)
        {
            std::string name;
            int length_in_days, score, best_before_day, nr_roles;
            auto skill_to_level = read_skills(nr_roles);
            projects.emplace_back(std::move(name), length_in_days, score, best_before_day, std::move(skill_to_level));
        }
        // Sort projects based on best before day
        std::sort(projects.begin(), projects.end(), [](const auto& lhs, const auto& rhs){
            return lhs.best_before_day < rhs.best_before_day;
        });
    }
};