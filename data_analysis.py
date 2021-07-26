#!/usr/bin/env python3
'''
filename.....: data_analysis.py
brief........: run the data analysis on all data collected, 
                and generate plots.
author.......: Feras Alshehri
email........: falshehri@mail.csuchico.edu
last modified: 5/212021
version......: 1.0
'''
import os
import sys
import json
import statistics
import performance_tester as pt
import numpy as np
import matplotlib.mlab as mlab
import matplotlib.pyplot as plt


def load_json_into_dict(json_path):
    '''
    loads a json file into a python dictionary.
    '''
    with open(json_path) as jf:
        dict = json.load(jf)

    return dict


def plot_and_save(plot_type, test_name="test_name", grrIter=-1,
                    x=[], y=[],
                    x_label="X-axis", y_label="Y-axis",
                    title="MyTitle", set_xticklabels=[]):
    '''
    generate plot and save as an image to local directory.
    '''
    supported_plots = ["hist", "box"]

    if type(y[0]) is type(3.4123456):
        mu = statistics.mean(y)
        sigma = statistics.stdev(y)
    else:
        mu = ""
        sigma = ""

    fig, ax = plt.subplots()

    if plot_type == "hist":
        # plot a histogram
        bins_count = 100
        n, bins, patches = ax.hist(y, density=False, bins=bins_count)
        # add bell curve
        _y = ((1 / (np.sqrt(2 * np.pi) * sigma)) *
        np.exp(-0.5 * (1 / sigma * (bins - mu))**2))
        ax.plot(bins, _y, '--')
        plt.title(f"{title} (n={len(y)}, $\mu$={mu:.3f}, $\sigma$={sigma:.3f})")
    elif plot_type == "box":
        bp = ax.boxplot(y, notch=True, showmeans=True)
        if mu == "":
            n = []
            _mu = []
            _sigma = []
            for i in range(len(y)): 
                n.append(len(y[i]))
                _mu.append(statistics.mean(y[i]))
                _sigma.append(statistics.stdev(y[i]))
            plt.title(f"{title} (n={sum(n)})")
            print(_mu)
            print(_sigma)
            
            for i, line in enumerate(bp['medians']):
                x, y = line.get_xydata()[1]
                text = f'   μ={_mu[i]:.3f}\n   σ={_sigma[i]:.3f}\n   n={n[i]}'
                ax.annotate(text, xy=(x, y))

            if len(set_xticklabels) > 0:
                ax.set_xticklabels(set_xticklabels)
        else:
            plt.title(f"{title} (n={len(y)}, $\mu$={mu:.3f}, $\sigma$={sigma:.3f})")
    else: 
        print(f"ERROR: Unsupported plot type requested (got {plot_type})")
        print(f"supported plot types are {supported_plots}")
        return False

    #  annotate plot
    plt.xlabel(x_label)
    plt.ylabel(y_label)
    if grrIter == -1:
        plot_path = os.path.join("stats", "plots",
                             f"{plot_type}_{title}.png")
    else:
        plot_path = os.path.join("stats", "plots", f"GR&R_{grrIter+1}",
                             f"{plot_type}_{title}_GR&R{grrIter}.png")
    
    # export plot
    plt.savefig(plot_path, transparent=True)

    # flush plot buffer and close it
    plt.clf()
    plt.close()

    return True


def remove_outliers(data):
    '''
    Remove outliers from a list of datapoints base don the given threshold.

    '''
    # calculate summary statistics
    data_mean, data_std = statistics.mean(data), statistics.stdev(data)
    
    # identify outliers
    cut_off = data_std * 1
    lower, upper = data_mean - cut_off, data_mean + cut_off
    
    # remove outliers
    data_fitted = [x for x in data if x > lower and x < upper]

    return data_fitted



def main():
    '''
    entry point of script
    '''
    # get command line args, if any
    grrIterations = 1
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        grrIterations = int(sys.argv[1])

    test_plan = pt.parse_test_plan()
    data = {}
    for test in test_plan:
        curr_test = test_plan[test]
        data[f"{curr_test['name']}_{curr_test['type']}_in_{curr_test['language']}"] = []

    for grrIter in range(grrIterations):
        for test in test_plan:
            curr_test = test_plan[test]
            if len(curr_test["executable_name"]) > 0:
                if grrIterations == 1:
                    stats_json_path = os.path.join("stats", "raw_data",
                                             f"{curr_test['name']}_{curr_test['statistics_output_file_name']}")
                else:
                    stats_json_path = os.path.join("stats", "raw_data", f"GR&R_{grrIter+1}",
                                             f"{curr_test['name']}_{curr_test['statistics_output_file_name']}")

                dict = load_json_into_dict(stats_json_path)

                test_name = f"{test}"
                if grrIterations == 1: grrIter = -1
                # plot a boxplot of execution time to visualize outliers
                plot_and_save(plot_type='box', grrIter=grrIter,
                                test_name=test_name, 
                                y=dict["summary"]["time data points"],
                                y_label="execution time [sec]",
                                title=f"{curr_test['name']}_{curr_test['type']}_{curr_test['language']}")
                # plot histogram of execution time
                plot_and_save(plot_type='hist', grrIter=grrIter,
                                test_name=test_name, 
                                y=dict["summary"]["time data points"],
                                x_label="exec time [sec]",
                                y_label="Probability",
                                title=f"{curr_test['name']}_{curr_test['type']}_{curr_test['language']}")

                # remove outliers (applying six-sigma/three stdev)
                # plot a boxplot of execution time fitted to a gaussian curve
                plot_and_save(plot_type='box', grrIter=grrIter,
                                test_name=test_name, 
                                y=remove_outliers(dict["summary"]["time data points"]),
                                y_label="execution time [sec]",
                                title=f"{curr_test['name']}_{curr_test['type']}_{curr_test['language']}_fitted")
                # plot histogram of execution time fitted to a gaussian curve
                plot_and_save(plot_type='hist', grrIter=grrIter,
                                test_name=test_name, 
                                y=remove_outliers(dict["summary"]["time data points"]),
                                x_label="exec time [sec]",
                                y_label="Probability",
                                title=f"{curr_test['name']}_{curr_test['type']}_{curr_test['language']}_fitted")

                # accumulate total data
                data[f"{curr_test['name']}_{curr_test['type']}_in_{curr_test['language']}"].append(list(remove_outliers(dict["summary"]["time data points"])))
        
            else:
                print(f"skipping [{curr_test['name']}_{curr_test['type']}_in_{curr_test['language']}] due to missing executable")

    # overview box plot (GR&R)
    for test in data:
        if len(data[test]) > 0:
            plot_and_save(plot_type='box',
                test_name="", 
                y=data[test],
                x_label="Test run #",
                y_label="execution time [sec]",
                title=f"Overview_{test}")
        else:
            print(f"skipping {[test]} due to missing executable")

    # MT vs MP c
    _d = [[],[]]
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multiprocessing_in_c"):
                for i in data[test]:
                    for x in i:
                        _d[0].append(x)
            
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_c"):
                for i in data[test]:
                    for x in i:
                        _d[1].append(x)
    print(_d)
    plot_and_save(plot_type='box',
        test_name="", 
        y=_d,
        x_label="Test run #",
        y_label="execution time [sec]",
        title=f"Overview_DNS_resolver_MTvsMP_in_c",
        set_xticklabels=["MP_C", "MT_C"])

    # MT c vs go
    _d = [[],[]]
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_c"):
                for i in data[test]:
                    for x in i:
                        _d[0].append(x)
            
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_go"):
                for i in data[test]:
                    for x in i:
                        _d[1].append(x)
    print(_d)
    plot_and_save(plot_type='box',
        test_name="", 
        y=_d,
        x_label="Test run #",
        y_label="execution time [sec]",
        title=f"Overview_DNS_resolver_MT_in_go_vs_c",
        set_xticklabels=["MT_C", "MT_Go"])

    # MT c vs python
    _d = [[],[]]
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_c"):
                for i in data[test]:
                    for x in i:
                        _d[0].append(x)
            
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_python"):
                for i in data[test]:
                    for x in i:
                        _d[1].append(x)
    print(_d)
    plot_and_save(plot_type='box',
        test_name="", 
        y=_d,
        x_label="Test run #",
        y_label="execution time [sec]",
        title=f"Overview_DNS_resolver_MT_in_c_vs_python",
        set_xticklabels=["MT_C", "MT_Py"])

    # MT go vs python
    _d = [[],[]]
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_go"):
                for i in data[test]:
                    for x in i:
                        _d[0].append(x)
            
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_python"):
                for i in data[test]:
                    for x in i:
                        _d[1].append(x)
    print(_d)
    plot_and_save(plot_type='box',
        test_name="", 
        y=_d,
        x_label="Test run #",
        y_label="execution time [sec]",
        title=f"Overview_DNS_resolver_MT_in_python_vs_c",
        set_xticklabels=["MT_Go", "MT_Py"])

    # MT vs SEQ c
    _d = [[],[]]
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("sequential_in_c"):
                for i in data[test]:
                    for x in i:
                        _d[0].append(x)
            
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_c"):
                for i in data[test]:
                    for x in i:
                        _d[1].append(x)
    print(_d)
    plot_and_save(plot_type='box',
        test_name="", 
        y=_d,
        x_label="Test run #",
        y_label="execution time [sec]",
        title=f"Overview_DNS_resolver_MT_vs_SEQ_in_C",
        set_xticklabels=["SEQ_C", "MT_C"])

    # MT vs SEQ go
    _d = [[],[]]
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("sequential_in_go"):
                for i in data[test]:
                    for x in i:
                        _d[0].append(x)
            
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_go"):
                for i in data[test]:
                    for x in i:
                        _d[1].append(x)
    print(_d)
    plot_and_save(plot_type='box',
        test_name="", 
        y=_d,
        x_label="Test run #",
        y_label="execution time [sec]",
        title=f"Overview_DNS_resolver_MT_vs_SEQ_in_Go",
        set_xticklabels=["SEQ_Go", "MT_Go"])

    # MT vs SEQ Py
    _d = [[],[]]
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("sequential_in_python"):
                for i in data[test]:
                    for x in i:
                        _d[0].append(x)
            
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_python"):
                for i in data[test]:
                    for x in i:
                        _d[1].append(x)
    print(_d)
    plot_and_save(plot_type='box',
        test_name="", 
        y=_d,
        x_label="Test run #",
        y_label="execution time [sec]",
        title=f"Overview_DNS_resolver_MT_vs_SEQ_in_Python",
        set_xticklabels=["SEQ_Py", "MT_Py"])

    
    # overview -- parallel
    _d = [[],[],[],[]]
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multiprocessing_in_c"):
                for i in data[test]:
                    for x in i:
                        _d[0].append(x)

    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_c"):
                for i in data[test]:
                    for x in i:
                        _d[1].append(x)

    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_go"):
                for i in data[test]:
                    for x in i:
                        _d[2].append(x)
            
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("multithreading_in_python"):
                for i in data[test]:
                    for x in i:
                        _d[3].append(x)
    print(_d)
    plot_and_save(plot_type='box',
        test_name="", 
        y=_d,
        x_label="Test run #",
        y_label="execution time [sec]",
        title=f"Overview_DNS_resolver",
        set_xticklabels=["MP_C", "MT_C", "MT_Go", "MT_Py"])

    # overview -- sequential
    _d = [[],[],[]]
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("sequential_in_c"):
                for i in data[test]:
                    for x in i:
                        _d[0].append(x)

    for test in data:
        if len(data[test]) > 0:
            if test.endswith("sequential_in_go"):
                for i in data[test]:
                    for x in i:
                        _d[1].append(x)
            
    for test in data:
        if len(data[test]) > 0:
            if test.endswith("sequential_in_python"):
                for i in data[test]:
                    for x in i:
                        _d[2].append(x)
    print(_d)
    plot_and_save(plot_type='box',
        test_name="", 
        y=_d,
        x_label="Test run #",
        y_label="execution time [sec]",
        title=f"Overview_DNS_resolver_Sequential",
        set_xticklabels=["SEQ_C", "SEQ_Go", "SEQ_Py"])



if __name__ == "__main__":
    main()