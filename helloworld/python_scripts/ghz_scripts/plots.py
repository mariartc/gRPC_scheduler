import json
import matplotlib.pyplot as plt


with open('results.json') as json_file:
    results = json.load(json_file)


def extract_details(string):
    string = string.replace("[", "")
    string = string.replace("]", "")
    return string.split(", ")


def plot(title, x, y, xlabel, ylabel, save_name, color, kind="plot", marker=[""], label=[""]):
    if kind == "plot":
        plt.figure()
        for i in range(len(y)):
            plt.plot(y[i], marker=marker[i], label=label[i], color=color[i])
        plt.xticks(range(len(x)), x)
    else:
        plt.figure(figsize=(25, 6))
        plt.bar(x, y[0], width=0.3, color=color[0])

    plt.title(title)
    plt.ylabel(ylabel)
    plt.xlabel(xlabel)
    if label != [""]:
        plt.legend()

    plt.savefig(f"../../ghz_plots/{save_name}", bbox_inches='tight')

    plt.clf()


def make_plots(changing_variable, times=10):
    keys = list(results[changing_variable].keys())
    [total, concurrency, cpus, data_type] = extract_details(keys[0])

    average_latency = []
    total_time = []
    rps = []

    for i in keys:
        average_latency.append(sum([results[changing_variable][i][str(j+1)]["average"] for j in range(times)])/(times*1000000))
        total_time.append(sum([results[changing_variable][i][str(j+1)]["total"] for j in range(times)])/(times*1000000))
        rps.append(sum([results[changing_variable][i][str(j+1)]["rps"] for j in range(times)])/times)

    total_time_calls = [i/int(total) for i in total_time]

    if changing_variable == "concurrency":
        x = [extract_details(i)[1] for i in keys]
        title = f" for {total} calls, {cpus} cpus and {data_type}"
        xlabel = "Concurrency"
        kind = "plot"
    elif changing_variable == "total":
        x = [extract_details(i)[0] for i in keys]
        title = f" for {concurrency} concurrency, {cpus} cpus and {data_type}"
        xlabel = "Total calls"
        kind = "plot"
        total_time_calls = [total_time[i]/int(x[i]) for i in range(len(x))]
    elif changing_variable == "cpus":
        x = [extract_details(i)[2] for i in keys]
        title = f" for {concurrency} concurrency, {total} calls and {data_type}"
        xlabel = "Number of cpu cores"
        kind = "plot"
    elif changing_variable == "data":
        x = [extract_details(i)[3] for i in keys]
        title = f" for {concurrency} concurrency, {total} calls and {cpus} cpus"
        xlabel = "Data type"
        kind = "bar"

    plot(title="Average latency"+title, \
        x=x, y=[average_latency], xlabel=xlabel, \
        ylabel='Average latency (ms)', save_name=f"average_latency/{changing_variable}.png", \
        color=["limegreen"], kind=kind)

    plot(title='Total time'+title, \
        x=x, y=[total_time], xlabel=xlabel, \
        ylabel='Total time (ms)', save_name=f"total_time/{changing_variable}.png", \
        color=["royalblue"], kind=kind)

    plot(title='Total_time/calls'+title, \
        x=x, y=[total_time_calls], xlabel=xlabel, \
        ylabel='Total_time/calls', save_name=f"total_time_calls/{changing_variable}.png", \
        color=["orange"], kind=kind)

    plot(title='Requests per second'+title, \
        x=x, y=[rps], xlabel=xlabel, \
        ylabel='RPS', save_name=f"rps/{changing_variable}.png", \
        color=["pink"], kind=kind)


def make_plots_multiplexing(changing_variable, times=10):
    keys = list(results[changing_variable].keys())
    [total, concurrency, cpus, _] = extract_details(keys[0])

    average_latency_float_number_list = []
    average_latency_long_string = []
    average_latency_float_number_list_multiplexing = []
    average_latency_long_string_multiplexing = []
    average_latency_multiplexing = []

    total_time_float_number_list = []
    total_time_long_string = []
    total_time_multiplexing = []

    rps_float_number_list = []
    rps_long_string = []
    rps_multiplexing = []

    for i in keys:
        [_, _, _, data_type] = extract_details(i)
        if data_type[:23] == "float_number_list_type_":
            average_latency_float_number_list.append(sum([results[changing_variable][i][str(j+1)]["average"] for j in range(times)])/(times*1000000))
            total_time_float_number_list.append(sum([results[changing_variable][i][str(j+1)]["total"] for j in range(times)])/(times*1000000))
            rps_float_number_list.append(sum([results[changing_variable][i][str(j+1)]["rps"] for j in range(times)])/times)
        elif data_type[:17] == "long_string_type_":
            average_latency_long_string.append(sum([results[changing_variable][i][str(j+1)]["average"] for j in range(times)])/(times*1000000))
            total_time_long_string.append(sum([results[changing_variable][i][str(j+1)]["total"] for j in range(times)])/(times*1000000))
            rps_long_string.append(sum([results[changing_variable][i][str(j+1)]["rps"] for j in range(times)])/times)
        else:
            average_latency_multiplexing.append(sum([results[changing_variable][i][str(j+1)]["average"] for j in range(times)])/(times*1000000))
            total_time_multiplexing.append(sum([results[changing_variable][i][str(j+1)]["total"] for j in range(times)])/(times*1000000))
            rps_multiplexing.append(sum([results[changing_variable][i][str(j+1)]["rps"] for j in range(times)])/times)

            sum_float_number_list = 0
            sum_long_string = 0
            for time in results[changing_variable][i].keys():
                sum_float_number_list_time, calls_float_number_list_time = 0, 0
                sum_long_string_time, calls_long_string_time = 0, 0
                average_time = results[changing_variable][i][time]["average"]
                for call in results[changing_variable][i][time]["details"]:
                    if call["latency"] > average_time:
                        sum_long_string_time += call["latency"]
                        calls_long_string_time += 1
                    else:
                        sum_float_number_list_time += call["latency"]
                        calls_float_number_list_time += 1
                sum_float_number_list += sum_float_number_list_time/calls_float_number_list_time
                sum_long_string += sum_long_string_time/calls_long_string_time
            average_latency_float_number_list_multiplexing.append(sum_float_number_list/(times*1000000))
            average_latency_long_string_multiplexing.append(sum_long_string/(times*1000000))

    total_time_float_number_list_calls = [i/int(total) for i in total_time_float_number_list]
    total_time_long_string_calls = [i/int(total) for i in total_time_long_string]
    total_time_multiplexing_calls = [i/int(total) for i in total_time_multiplexing]

    if changing_variable == "concurrency":
        x = [extract_details(i)[1] for i in keys[:(len(keys)//3)]]
        title = f" for {total} calls, {cpus} cpus"
        xlabel = "Concurrency"
    elif changing_variable == "total":
        x = [extract_details(i)[0] for i in keys[:(len(keys)//3)]]
        title = f" for {concurrency} concurrency, {cpus} cpus"
        xlabel = "Total number of calls"
        total_time_float_number_list_calls = [total_time_float_number_list[i]/int(x[i]) for i in range(len(x))]
        total_time_long_string_calls = [total_time_long_string[i]/int(x[i]) for i in range(len(x))]
        total_time_multiplexing_calls = [total_time_multiplexing[i]/int(x[i]) for i in range(len(x))]
    elif changing_variable == "cpus":
        x = [extract_details(i)[2] for i in keys[:(len(keys)//3)]]
        title = f" for {concurrency} concurrency, {total} calls"
        xlabel = "Number of cpu cores"
        kind = "plot"

    # rps
    plot(f"Rps{title}", x, \
        [rps_float_number_list, rps_long_string, rps_multiplexing], xlabel, \
        "Rps", f"multiplexing/{changing_variable}/rps", ["limegreen", "royalblue", "tomato"], \
        "plot", ["o", "x", "."], ['float_number_list', 'long_string', 'multiplexing'])

    # total time
    plot(f"Total_time{title}", x, \
        [total_time_float_number_list, total_time_long_string, total_time_multiplexing], \
        xlabel, "Total_time", f"multiplexing/{changing_variable}/total_time", \
        ["limegreen", "royalblue", "tomato"], "plot", ["o", "x", "."], \
        ['float_number_list', 'long_string', 'multiplexing'])

    # total time / calls
    plot(f"Total_time/calls{title}", x, \
        [total_time_float_number_list_calls, total_time_long_string_calls, total_time_multiplexing_calls], \
        xlabel, "Total_time/calls", f"multiplexing/{changing_variable}/total_time_calls", \
        ["limegreen", "royalblue", "tomato"], "plot", ["o", "x", "."], \
        ['float_number_list', 'long_string', 'multiplexing'])

    # average latency
    plot(f"Average latency{title}", x, \
        [average_latency_float_number_list, average_latency_long_string, average_latency_multiplexing, \
        average_latency_float_number_list_multiplexing, average_latency_long_string_multiplexing], \
        xlabel, "Average latency", f"multiplexing/{changing_variable}/average_latency", \
        ["limegreen", "royalblue", "tomato", "orange", "pink"], "plot", ["o", "x", ".", "v", "^"], \
        ['float_number_list', 'long_string', 'multiplexing', 'float_number_list_multiplexing', 'long_string_multiplexing'])



# # CONCURRENCY
# make_plots("concurrency")

# # TOTAL
# make_plots("total")

# # CPUS
# make_plots("cpus")

# # DATA
# make_plots("data")

# MULTIPLEXING
make_plots_multiplexing("concurrency", 5)
make_plots_multiplexing("total", 5)
make_plots_multiplexing("cpus", 5)
