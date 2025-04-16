#include <array>
#include <iostream>
#include "Data.h"


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
    }
    return 0;
}
