#include <set>
#include <array>
#include <iostream>
#include "Data.h"

#include "BS_thread_pool.hpp"

using ProjectAllocation = std::pair<std::string, std::vector<std::string>>;

BS::thread_pool pool(9);
int best_score;
std::vector<ProjectAllocation> best_allocation;
using namespace std::chrono_literals;

struct SimulationState
{
    int day, score_so_far;
    std::set<Contributor> contributors;
    std::vector<ProjectAllocation> proj_to_contrib;
    std::map<std::string, std::vector<std::string>> skill_to_contrib;
    std::map<std::string, int> contrib_to_availability;

    SimulationState(const std::vector<Contributor>& contributors)
    : day(0)
    , score_so_far(0)
    , contributors(std::set(contributors.begin(), contributors.end()))
    {}
};

std::vector<std::future<SimulationState>> futures;
std::mutex futures_mutex;

std::vector<ProjectAllocation> get_top_n_available_allocations(const Data& data, const SimulationState& simulation_state, int n)
{
    std::vector<ProjectAllocation> results;
    // ...
    return results;
}

SimulationState simulate(const Data& data, SimulationState simulation_state, int k)
{
    int current_step = 0;
    while (true)
    {
        if (current_step == k)
        {
            const auto top_n_allocations = get_top_n_available_allocations(data, simulation_state, 5);
            if (top_n_allocations.empty())
                break;
            // Split in 5, append to threadpool

            current_step = 0;
        }
        else
        {
            const auto top_1_allocation = get_top_n_available_allocations(data, simulation_state, 1);
            if (top_1_allocation.empty())
                break;
            // Update contributor's skill, score, etc
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

void add_task_to_pool(const Data&data, const SimulationState& simulation_state)
{
    std::lock_guard<std::mutex> lock(futures_mutex);

    auto future_for_task = pool.submit_task([&]() { return simulate(data, simulation_state, 10); });
    futures.push_back(std::move(future_for_task));
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

        std::this_thread::sleep_for(10000ms);
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
