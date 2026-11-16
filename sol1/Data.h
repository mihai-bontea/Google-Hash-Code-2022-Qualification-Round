#pragma once
#include <vector>
#include <fstream>
#include <algorithm>
#include <ankerl/unordered_dense.h>

#define MAX_PROJECTS 19413

template<typename K, typename V>
using umap = ankerl::unordered_dense::map<K, V>;

template<typename T>
using uset = ankerl::unordered_dense::set<T>;

using ContributorsPerSkill = std::array<uset<int>, 21>;
using ProjectAllocation = std::pair<int, std::vector<int>>;

struct Project
{
    std::string name;
    int length_in_days, score, best_before_day;
    std::vector<std::pair<int, int>> skill_to_level;

    Project(std::string&& name,
            int length_in_days,
            int score,
            int best_before_day,
            std::vector<std::pair<int, int>>&& skill_to_level)
            : name(std::move(name))
            , length_in_days(length_in_days)
            , score(score)
            , best_before_day(best_before_day)
            , skill_to_level(std::move(skill_to_level))
    {}
};

class Data
{
private:
    inline void build_skill_to_contributors(umap<int, ContributorsPerSkill> skill_to_contributors_temp)
    {
        skill_to_contributors.reserve(skill_to_contributors_temp.size());
        for (int skill_id = 0; skill_id < skill_to_contributors_temp.size(); ++skill_id)
        {
            skill_to_contributors.push_back(std::move(skill_to_contributors_temp[skill_id]));
        }
    }

public:
    std::vector<std::string> id_to_contributor_name;

    int nr_contributors, nr_projects;
    std::vector<ContributorsPerSkill> skill_to_contributors;

    std::vector<Project> projects;

    Data(const std::string& filename)
    {
        int current_skill_id = 0;

        std::ifstream fin(filename);
        fin >> nr_contributors >> nr_projects;

        // avoid re-allocations and over-allocations
        projects.reserve(nr_projects);
        id_to_contributor_name.reserve(nr_contributors);

        ankerl::unordered_dense::map<std::string, int> skill_to_id_temp;
        umap<int, ContributorsPerSkill> skill_to_contributors_temp;

        std::vector<uset<int>> contr_to_skills;

        // Read the contributor info
        int current_contributor_id = 0;
        for (int contr_index = 0; contr_index < nr_contributors; ++contr_index)
        {
            std::string contributor_name;
            int nr_skills;

            fin >> contributor_name >> nr_skills;
            id_to_contributor_name.push_back(std::move(contributor_name));

            uset<int> skills;
            while (nr_skills--)
            {
                std::string skill_name;
                int skill_level;
                fin >> skill_name >> skill_level;

                if (!skill_to_id_temp.contains(skill_name))
                {
                    skill_to_id_temp[skill_name] = current_skill_id++;
                }
                const int skill_id = skill_to_id_temp[skill_name];

                skills.insert(skill_id);
                skill_to_contributors_temp[skill_id][skill_level].insert(current_contributor_id);
            }
            contr_to_skills.push_back(std::move(skills));

            current_contributor_id++;
        }
        // Read the project info
        for (int proj_index = 0; proj_index < nr_projects; ++proj_index)
        {
            std::string name;
            int length_in_days, score, best_before_day, nr_roles;
            std::vector<std::pair<int, int>> skill_to_level;
            fin >> name >> length_in_days >> score >> best_before_day >> nr_roles;
            skill_to_level.reserve(nr_roles);

            while (nr_roles--)
            {
                std::string skill_name;
                int skill_level;
                fin >> skill_name >> skill_level;

                if (!skill_to_id_temp.contains(skill_name))
                {
                    skill_to_id_temp[skill_name] = current_skill_id++;
                }
                const int skill_id = skill_to_id_temp[skill_name];

                skill_to_level.emplace_back(skill_id, skill_level);
            }

            projects.emplace_back(std::move(name), length_in_days, score, best_before_day, std::move(skill_to_level));
        }

        // No skill in input means skill at level 0(can be improved with mentoring)
        for (int contrib_id = 0; contrib_id < nr_contributors; ++contrib_id)
            for (auto& [skill_name, contr_to_skills_per_level] : skill_to_contributors_temp)
                if (!contr_to_skills[contrib_id].contains(skill_name))
                {
                    auto& level0_set_for_skill = contr_to_skills_per_level[0];
                    level0_set_for_skill.insert(contrib_id);
                }

        // Build the vector based on the map
        build_skill_to_contributors(std::move(skill_to_contributors_temp));

        // Sort projects based on best before day
        std::sort(projects.begin(), projects.end(), [](const auto& lhs, const auto& rhs){
            return lhs.best_before_day < rhs.best_before_day;
        });
    }

    void write_to_file(const std::string& filename, const std::vector<ProjectAllocation>& allocation) const
    {
        std::ofstream fout(filename);
        fout << allocation.size() << '\n';
        for (const auto& [project_id, contributor_ids] : allocation)
        {
            fout << projects[project_id].name << '\n';
            for (const auto contributor_id : contributor_ids)
                fout << id_to_contributor_name[contributor_id] << " ";
            fout << '\n';
        }
    }
};