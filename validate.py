
solutions = ["sol1"]
input_files = ["a_an_example", "b_better_start_small", "c_collaboration", 
               "d_dense_schedule", "e_exceptional_skills", "f_find_great_mentors"]


class ProjectInfo:
    def __init__(self, length_in_days, score, best_before, skills):
        self.length_in_days = length_in_days
        self.score = score
        self.best_before = best_before
        self.skills = skills


def read_input_file(filename):
    with open(filename, 'r') as fin:
        nr_contrib, nr_proj = list(map(int, fin.readline().split()))

        # Reading contributor information
        contrib_to_skills = {
            (name := line.split()[0]): {
                skill: int(level) 
                for skill, level in (fin.readline().split() for _ in range(int(line.split()[1])))
            }
        for line in (fin.readline() for _ in range(nr_contrib))
        }
        
        # Assign skill with level 0 as a default
        all_skills = {skill for skills in contrib_to_skills.values() for skill in skills}
        for skills in contrib_to_skills.values():
            for skill in all_skills:
                skills.setdefault(skill, 0)
        
        # Reading project information
        proj_name_to_info = {
            name: ProjectInfo(int(length_in_days), int(score), int(best_before), 
                              [(skill, int(level)) for skill, level in (fin.readline().split() for _ in range(int(nr_roles)))])
            for name, length_in_days, score, best_before, nr_roles in (fin.readline().split() for _ in range(nr_proj))
        }
    return nr_contrib, nr_proj, contrib_to_skills, proj_name_to_info


def read_solution_file(filename):
    with open(filename, 'r') as fin:
        nr_proj_planned = int(fin.readline())

        proj_to_contrib = {
            fin.readline().strip(): fin.readline().split()
            for _ in range(nr_proj_planned)
        }
    return nr_proj_planned, proj_to_contrib


for solution in solutions:
    print(f"For {solution} we got:")    
    for input_file in input_files:
        full_input_path = f"input_files/{input_file}.in"
        nr_contrib, nr_proj, contrib_to_skills, proj_name_to_info = read_input_file(full_input_path)

        try:
            full_solution_path = f"output_files/{solution}/{input_file}.out"
            nr_proj_planned, proj_to_contrib = read_solution_file(full_solution_path)
            # No project should appear twice in output
            assert(nr_proj_planned == len(proj_to_contrib))

            busy_until = {contrib: 0 for contrib in contrib_to_skills}
            simulation_day = score = skill_increase = 0
            for proj_name, contrib_names in proj_to_contrib.items():

                # The project can be started once all involved contributors finished their previous project
                starting_day = max(busy_until[contrib_name] for contrib_name in contrib_names)
                simulation_day = max(simulation_day, starting_day)

                # Check that contributor can fill that role
                count = 0
                for contrib_name in contrib_names:
                    skill_name, skill_level = proj_name_to_info[proj_name].skills[count]
                    assert(contrib_to_skills[contrib_name][skill_name] >= skill_level - 1)
                    # Contributor requires mentorship
                    if contrib_to_skills[contrib_name][skill_name] == skill_level - 1:
                        # Ensure that someone can provide mentorship
                        assert any(contrib_to_skills[name][skill_name] >= skill_level for name in contrib_names)

                        # Level up the contributor in that skill if needed
                        contrib_to_skills[contrib_name][skill_name] += 1
                        skill_increase += 1
                    count += 1

                # Score the project
                penalty = max(0, simulation_day + proj_name_to_info[proj_name].length_in_days - proj_name_to_info[proj_name].best_before)
                score += max(0, proj_name_to_info[proj_name].score - penalty)
            
            print(f"--->For {input_file} we obtained score = {score:,}, skill_increase = {skill_increase:,}.\n")
        
        except FileNotFoundError:
            print(f"Error: File '{full_solution_path}' not found.")