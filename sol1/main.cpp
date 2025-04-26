#include <set>
#include <array>
#include <bitset>
#include <iostream>
#include "Data.h"
#include "ProjectAllocator.h"

ProjectAllocation get_best_allocation(const Data& data, SimulationState& simulation_state, int n)
{
    std::vector<ProjectAllocation> results;

    auto it = std::lower_bound(
            data.projects.begin(), data.projects.end(), simulation_state.day,
            [](const Project& p, int day) {
                return p.best_before_day < day;
            }
    );

    int lower_bound = it - data.projects.begin();
    int upper_bound = data.nr_projects;

    std::priority_queue<std::pair<int, int>> pq;
    for (int project_index = lower_bound; project_index < upper_bound; ++project_index)
        if (simulation_state.can_project_be_done(project_index))
            pq.emplace(simulation_state.heuristic_score(project_index), project_index);

    WeightedAllocation best_allocation;
    auto start = steady_clock::now();
    while (!pq.empty())
    {
        auto [approx_score, project_index] = pq.top();
        pq.pop();

        const auto& project = data.projects[project_index];
        auto [learning_points, contributor_names] = ProjectAllocator::find_allocation_for_project(simulation_state, project);

        if (!contributor_names.empty())
        {
            if (!simulation_state.actual_score(project_index) && !learning_points)
                simulation_state.project_done[project_index] = true;
            else
            {
                best_allocation = {project_index, contributor_names};
                break;
            }
        }
    }
    auto now = steady_clock::now();
    auto elapsed = duration_cast<seconds>(now - start);
    std::cout << "Got a new allocation in " << elapsed.count() << " seconds.\n";

    return best_allocation;
}

SimulationState simulate(const Data& data, SimulationState simulation_state)
{
    while (simulation_state.day != NMAX)
    {
        auto best_allocation = get_best_allocation(data, simulation_state, 5);

        const auto& [project_id, contributor_names] = best_allocation;

        // Pass days if nothing more can be done at this day
        if (contributor_names.empty())
        {
            simulation_state.pass_days();
            std::cout << "Passed day to " << simulation_state.day << std::endl;
            continue;
        }

        simulation_state.add_allocation(best_allocation);
        ProjectAllocator::update_contributors(data, simulation_state, best_allocation);

        const auto& project = data.projects[project_id];
        int penalty = (project.best_before_day > simulation_state.day + project.length_in_days)? 0 : simulation_state.day + project.length_in_days - project.best_before_day;
        int actual_score = std::max(0, (project.score - penalty));
        simulation_state.score_so_far += actual_score;
    }
    return simulation_state;
}

int main()
{
    const std::string in_prefix = "../../input_files/";
    const std::string out_prefix = "../../output_files/sol1/";
    const std::array<std::string, 6> input_files = {"a_an_example.in", "b_better_start_small.in", "c_collaboration.in",
                                          "d_dense_schedule.in", "e_exceptional_skills.in", "f_find_great_mentors.in"};

    for (const auto& input_file : input_files)
    {
        Data data(in_prefix + input_file);

        std::cout << "Successfully read " << data.nr_contributors << " contributors, and " << data.nr_projects << " projects\n";

        SimulationState base_simulation(data, data.skill_to_contributors);
        auto result = simulate(data, base_simulation);

        const auto out_filename = out_prefix + input_file.substr(0, (input_file.find('.'))) + ".out";
        data.write_to_file(out_filename, result.proj_to_contrib);
    }
    return 0;
}