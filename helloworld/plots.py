import json
import matplotlib.pyplot as plt


with open('results.json') as json_file:
    results = json.load(json_file)


def extract_details(string):
    string = string.replace("[", "")
    string = string.replace("]", "")
    return string.split(", ")


def plot(title, x, y, xlabel, ylabel, save_name, color, kind="plot"):
    if kind == "plot":
        plt.figure()
        plt.plot(y, color=color)
        plt.xticks(range(len(x)), x)
    else:
        plt.figure(figsize=(25, 6))
        plt.bar(x, y, width=0.3, color=color)

    plt.title(title)
    plt.ylabel(ylabel)
    plt.xlabel(xlabel)

    plt.savefig(f"plots/{save_name}", bbox_inches='tight')

    plt.clf()


def make_plots(changing_variable):
    keys = list(results[changing_variable].keys())
    [total, concurrency, cpus, data_type] = extract_details(keys[0])

    average_latency = []
    total_time = []
    rps = []

    for i in keys:
        average_latency.append(sum([results[changing_variable][i][str(j+1)]["average"] for j in range(10)])/(10*1000000))
        total_time.append(sum([results[changing_variable][i][str(j+1)]["total"] for j in range(10)])/(10*1000000))
        rps.append(sum([results[changing_variable][i][str(j+1)]["rps"] for j in range(10)])/10)

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
        x=x, y=average_latency, xlabel=xlabel, \
        ylabel='Average latency (ms)', save_name=f"average_latency/{changing_variable}.png", \
        color="limegreen", kind=kind)

    plot(title='Total time'+title, \
        x=x, y=total_time, xlabel=xlabel, \
        ylabel='Total time (ms)', save_name=f"total_time/{changing_variable}.png", \
        color="royalblue", kind=kind)

    plot(title='Total_time/calls'+title, \
        x=x, y=total_time_calls, xlabel=xlabel, \
        ylabel='Total_time/calls', save_name=f"total_time_calls/{changing_variable}.png", \
        color="orange", kind=kind)

    plot(title='Requests per second'+title, \
        x=x, y=rps, xlabel=xlabel, \
        ylabel='RPS', save_name=f"rps/{changing_variable}.png", \
        color="pink", kind=kind)


def make_plots_multiplexing():
    times = 2
    keys = list(results["concurrency"].keys())
    [total, _, cpus, _] = extract_details(keys[0])

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
            average_latency_float_number_list.append(sum([results["concurrency"][i][str(j+1)]["average"] for j in range(times)])/(times*1000000))
            total_time_float_number_list.append(sum([results["concurrency"][i][str(j+1)]["total"] for j in range(times)])/(times*1000000))
            rps_float_number_list.append(sum([results["concurrency"][i][str(j+1)]["rps"] for j in range(times)])/times)
        elif data_type[:17] == "long_string_type_":
            average_latency_long_string.append(sum([results["concurrency"][i][str(j+1)]["average"] for j in range(times)])/(times*1000000))
            total_time_long_string.append(sum([results["concurrency"][i][str(j+1)]["total"] for j in range(times)])/(times*1000000))
            rps_long_string.append(sum([results["concurrency"][i][str(j+1)]["rps"] for j in range(times)])/times)
        else:
            average_latency_multiplexing.append(sum([results["concurrency"][i][str(j+1)]["average"] for j in range(times)])/(times*1000000))
            total_time_multiplexing.append(sum([results["concurrency"][i][str(j+1)]["total"] for j in range(times)])/(times*1000000))
            rps_multiplexing.append(sum([results["concurrency"][i][str(j+1)]["rps"] for j in range(times)])/times)

            sum_float_number_list = 0
            sum_long_string = 0
            for time in results["concurrency"][i].keys():
                sum_float_number_list_time, calls_float_number_list_time = 0, 0
                sum_long_string_time, calls_long_string_time = 0, 0
                average_time = results["concurrency"][i][time]["average"]
                for call in results["concurrency"][i][time]["details"]:
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

    x = [1, 5, 10, 50, 100, 150, 200]
    title = f" for {total} calls, {cpus} cpus and {data_type}"
    xlabel = "Concurrency"

    # rps
    plt.figure()
    plt.plot(rps_float_number_list, marker="o", label='float_number_list', color="limegreen")
    plt.plot(rps_long_string, marker="x", label='long_string', color="royalblue")
    plt.plot(rps_multiplexing, marker=".", label='multiplexing', color="tomato")
    plt.xticks(range(len(x)), x)
    plt.title(f"Rps for {total} calls, {cpus} cpus")
    plt.ylabel("Rps")
    plt.xlabel("Concurrency")
    plt.legend()
    plt.savefig(f"plots/multiplexing/concurrency/rps", bbox_inches='tight')
    plt.clf()

    # total time
    plt.figure()
    plt.plot(total_time_float_number_list, marker="o", label='float_number_list', color="limegreen")
    plt.plot(total_time_long_string, marker="x", label='long_string', color="royalblue")
    plt.plot(total_time_multiplexing, marker=".", label='multiplexing', color="tomato")
    plt.xticks(range(len(x)), x)
    plt.title(f"Total_time/calls for {total} calls, {cpus} cpus")
    plt.ylabel("Total_time/calls")
    plt.xlabel("Concurrency")
    plt.legend()
    plt.savefig(f"plots/multiplexing/concurrency/total_time", bbox_inches='tight')
    plt.clf()

    # total time / calls
    plt.figure()
    plt.plot(total_time_float_number_list_calls, marker="o", label='float_number_list', color="limegreen")
    plt.plot(total_time_long_string_calls, marker="x", label='long_string', color="royalblue")
    plt.plot(total_time_multiplexing_calls, marker=".", label='multiplexing', color="tomato")
    plt.xticks(range(len(x)), x)
    plt.title(f"Total_time/calls for {total} calls, {cpus} cpus")
    plt.ylabel("Total_time/calls")
    plt.xlabel("Concurrency")
    plt.legend()
    plt.savefig(f"plots/multiplexing/concurrency/total_time_calls", bbox_inches='tight')
    plt.clf()

    # average latency
    plt.figure()
    plt.plot(average_latency_float_number_list, marker="o", label='float_number_list', color="limegreen")
    plt.plot(average_latency_long_string, marker="x", label='long_string', color="royalblue")
    plt.plot(average_latency_multiplexing, marker=".", label='multiplexing', color="tomato")
    plt.plot(average_latency_float_number_list_multiplexing, marker="v", label='float_number_list_multiplexing', color="orange")
    plt.plot(average_latency_long_string_multiplexing, marker="^", label='long_string_multiplexing', color="pink")
    plt.xticks(range(len(x)), x)
    plt.title(f"Average latency for {total} calls, {cpus} cpus")
    plt.ylabel("Average latency")
    plt.xlabel("Concurrency")
    plt.legend()
    plt.savefig(f"plots/multiplexing/concurrency/average_latency", bbox_inches='tight')
    plt.clf()



# # CONCURRENCY
# make_plots("concurrency")

# # TOTAL
# make_plots("total")

# # CPUS
# make_plots("cpus")

# # DATA
# make_plots("data")

make_plots_multiplexing()
