#include <set>
#include <array>
#include <bitset>
#include <iostream>
#include "Data.h"
#include "BoundedPriorityQueue.h"
#include "ProjectAllocator.h"
#include "BS_thread_pool.hpp"

BS::thread_pool pool(9);
int best_score;
std::vector<ProjectAllocation> best_allocation;
using namespace std::chrono_literals;

std::vector<std::future<SimulationState>> futures;
std::mutex futures_mutex;

std::vector<ProjectAllocation> get_top_n_available_allocations(const Data& data, SimulationState& simulation_state, int n)
{
    std::vector<ProjectAllocation> results;

    // TODO: I could give additional score to a project if it can be used to increase a high-demand skill

    using ScoreForAllocation = std::pair<int, ProjectAllocation>;
    struct CompareScore {
        bool operator()(const ScoreForAllocation& lhs, const ScoreForAllocation& rhs) const {
            return lhs.first > rhs.first; // min-heap
        }
    };

    BoundedPriorityQueue<ScoreForAllocation, CompareScore> top_n_queue(n);

    int lower_bound = 0;
    int upper_bound = data.nr_projects;

    auto compute_score = [&simulation_state](const Project& project){
        return std::max(0, project.score - simulation_state.day);
    };

    for (int project_index = lower_bound; project_index < upper_bound; ++project_index)
    {
        const auto& project = data.projects[project_index];
        auto contributor_ids = ProjectAllocator::find_allocation_for_project(simulation_state, project);

        int score = compute_score(project);
        if (!contributor_ids.empty() && score > 0)
            top_n_queue.push({score, {project_index, contributor_ids}});
    }

    auto top_n_sorted = top_n_queue.extract_sorted();
    for (const auto& [score, alloc] : top_n_sorted)
        results.push_back(alloc);

    return results;
}

SimulationState simulate(const Data& data, SimulationState simulation_state, int k);

void add_task_to_pool(const Data&data, const SimulationState& simulation_state)
{
    std::lock_guard<std::mutex> lock(futures_mutex);

    auto future_for_task = pool.submit_task([&]() { return simulate(data, simulation_state, 10); });
    futures.push_back(std::move(future_for_task));
}

SimulationState simulate(const Data& data, SimulationState simulation_state, int k)
{
    int current_step = 0;
    while (true)
    {
        bool allocation_failed = false;
        if (current_step == k)
        {
            current_step = 0;
            continue;

            auto top_n_allocations = get_top_n_available_allocations(data, simulation_state, 5);
            if (!top_n_allocations.empty())
            {
                allocation_failed = true;
            }
            else
            {
                auto top_1_allocation = top_n_allocations.front();
                top_n_allocations.erase(top_n_allocations.begin());

                for (const auto& allocation : top_n_allocations)
                {
                    auto new_simulation_state = simulation_state;
                    new_simulation_state.add_allocation(allocation);

                    ProjectAllocator::update_contributors(data, new_simulation_state, allocation);
                    add_task_to_pool(data, new_simulation_state);
                }
                simulation_state.add_allocation(top_1_allocation);
                ProjectAllocator::update_contributors(data, simulation_state, top_1_allocation);
                current_step = 0;
            }
        }
        else
        {
            const auto top_n_allocations = get_top_n_available_allocations(data, simulation_state, 1);
            if (top_n_allocations.empty())
            {
                allocation_failed = true;
            }
            else
            {
                const auto& top_1_allocation = top_n_allocations.front();
                simulation_state.add_allocation(top_1_allocation);
                ProjectAllocator::update_contributors(data, simulation_state, top_1_allocation);
                current_step++;
            }
        }

        if (allocation_failed)
        {
            int prev_day = simulation_state.day;
            simulation_state.pass_days();
            if (prev_day == simulation_state.day)
                break;
        }
    }
    return simulation_state;
}

void poll_finished_futures()
{
    std::lock_guard<std::mutex> lock(futures_mutex);

    auto it = futures.begin();
    while (it != futures.end())
    {
        if (it->wait_for(0ms) == std::future_status::ready)
        {
            auto result = it->get();
            std::cout << "Got result!\n";
            if (result.score_so_far > best_score)
            {
                best_score = result.score_so_far;
                best_allocation = result.proj_to_contrib;
            }

            it = futures.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void solve(const Data& data)
{
    SimulationState base_simulation(data.contributors);
    add_task_to_pool(data, base_simulation);

    while (true)
    {
        poll_finished_futures();
        {
            std::lock_guard<std::mutex> lock(futures_mutex);
            if (futures.empty())
                break;
        }

        std::this_thread::sleep_for(5000ms);
    }
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

        solve(data);
    }
    return 0;
}