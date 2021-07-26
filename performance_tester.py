'''
filename.....: performance_tester.py
brief........: run all test recipes, capture measurements of each test,
                and generate statistical values to be used for 
                benchmark analaysis.
author.......: Feras Alshehri
email........: falshehri@mail.csuchico.edu
last modified: 5/20/2021
version......: 1.0
'''
import json
import os
import sys
import time
import statistics

suppress_programs_output = True

def parse_test_plan(test_manifest = "test_recipes.json"):
    '''
    parse the test plan from a test manifest json file.
    '''
    with open(test_manifest) as jf:
        recipe = json.load(jf)

    return recipe


def parse_dns_out_file(out_file_path = 'out.txt'):
    '''
    Extrapolate statistics from output file of the program.
    '''
    with open(out_file_path, "r") as f:
        total = positive = unhandled = error = 0
        for line in f:
            if line != "\n": 
                total += 1
                if line[-2].isdigit(): positive += 1
                if line.endswith("UNHANDELED\n"): unhandled += 1
                if line.endswith(",\n"): error += 1
    
    rc = [total, positive, unhandled, error]

    return rc


def calculate_stats_summary(dict):
    ''' 
    calculate the stats summary of all runs.
    '''
    time_values = []
    total_hits = []
    errors = []

    for run in dict:
            for run_details in dict[run]:
                if run_details == "execution time":
                    time_values.append(dict[run][run_details])
                elif run_details == "number of total hits":
                    total_hits.append(dict[run][run_details])
                elif run_details == "number of error hits":
                    errors.append(dict[run][run_details])
                else:
                    continue

    if len(time_values) > 1:
        dict["summary"] = {
            "time data points" : time_values,
            "total hits" : total_hits,
            "errors" : errors,
            "Mean execution time" : statistics.fmean(time_values),
            "Median execution time" : statistics.median(time_values),
            "Standard deviation execution time" : statistics.stdev(time_values),
            "Variance execution time" : statistics.variance(time_values)
        }
    else:
        dict["summary"] = {
            "time data points" : time_values,
            "total hits" : total_hits,
            "errors" : errors,
            "Mean execution time" : statistics.fmean(time_values),
            "Median execution time" : statistics.median(time_values),
            "Standard deviation execution time" : None,
            "Variance execution time" : None
        }

    return dict



def write_stats_to_file(dict, stats_file):
    '''
    Write statistics to a text file.
    '''
    dict = calculate_stats_summary(dict)

    with open(stats_file, "w") as f:
        json.dump(dict, f, indent=4)
    
    print(f"Successfully wrote all statistics in {os.path.abspath(stats_file)}")

    return


def assemble_command(recipe):
    '''
    Assemble command to run an executable from a recipe.
    '''
    # ensure a named executable is available
    if len(recipe['executable_name']) == 0:
        return ""

    # check if we are trying to run a python script
    # TODO: root-cause why we can't run it as an executable
    cmd = ""
    if recipe['executable_name'].endswith('.py'):
        cmd = "python3 "

    # path to executable relative to project root folder
    cmd += os.path.join(recipe['name'], recipe['type'], 
                    recipe['language'], recipe['executable_name'])

    # add arguments (input and output files)    
    for i in recipe['input_files_names']:
        cmd += " "
        cmd += os.path.join(f"{recipe['name']}", "input", f"{i}")
    cmd += os.path.join(f" {recipe['name']}", "output", f"{recipe['output_file_name']}")

    return cmd


def run_executable(cmd, n, stats_file):
    '''
    run the cmd command.
    '''
    stats = {}

    for i in range(n):
        print(".", end="", flush=True)
        # time.sleep(1)

        # initial time
        t_i = time.time()

        if suppress_programs_output: 
            # supress stdout and stderr
            exit_status = os.system(cmd+" > /dev/null 2>&1")
        else:
            exit_status = os.system(cmd)

        # final time
        t_f = time.time()

        if not exit_status:
            total, positive, unhandled, error = parse_dns_out_file(cmd.split()[-1])

            stats[i] = {"execution time": t_f-t_i,
                        "number of total hits": total,
                        "number of positive hits": positive,
                        "number of unhandled hits": unhandled,
                        "number of error hits": error}
        else:
            print(f"Check your command (got [{cmd}]")
            break

    if len(stats) > 0:
        print("Done!")
        write_stats_to_file(stats, stats_file)

    print(f"Total execution time = {t_f-t_i} seconds")

    return


def main():
    '''
    Entry point of the function.
    '''
    test_plan = parse_test_plan()

    grrIterations = 1
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        grrIterations = int(sys.argv[1])
    
    print(f"{grrIterations} grr iterations requested")
    time.sleep(1)

    for grr in range(grrIterations):
        for test in test_plan:
            # time.sleep(1)
            curr_test = test_plan[test]
            cmd = assemble_command(curr_test)
            if cmd == "":
                print(f"skipping {test} due to missing executable name")
            else:
                print(f"running {test} ({curr_test['name']}, {curr_test['type']}, {curr_test['language']})",
                        end="", flush=True)
                if grrIterations > 1:
                    stats_file = os.path.join("stats", "raw_data", f"GR&R_{grr+1}",
                                        f"{curr_test['name']}_{curr_test['statistics_output_file_name']}")
                else:
                    stats_file = os.path.join("stats", "raw_data",
                                        f"{curr_test['name']}_{curr_test['statistics_output_file_name']}")
                run_executable(cmd, curr_test['iterations'],
                                stats_file)


if __name__ == "__main__":
    main()