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

    average_latency = []
    total_time = []
    rps = []

    for i in keys:
        average_latency.append(sum([results[changing_variable][i][str(j+1)]["average"] for j in range(10)])/(10*1000000))
        total_time.append(sum([results[changing_variable][i][str(j+1)]["total"] for j in range(10)])/(10*1000000))
        rps.append(sum([results[changing_variable][i][str(j+1)]["rps"] for j in range(10)])/10)

    plot(title="Average latency"+title, \
        x=x, y=average_latency, xlabel=xlabel, \
        ylabel='Average latency (ms)', save_name=f"average_latency/{changing_variable}.png", \
        color="limegreen", kind=kind)

    plot(title='Total time'+title, \
        x=x, y=total_time, xlabel=xlabel, \
        ylabel='Total time (ms)', save_name=f"total_time/{changing_variable}.png", \
        color="royalblue", kind=kind)

    plot(title='Requests per second'+title, \
        x=x, y=rps, xlabel=xlabel, \
        ylabel='RPS', save_name=f"rps/{changing_variable}.png", \
        color="pink", kind=kind)



# CONCURRENCY
make_plots("concurrency")

# TOTAL
make_plots("total")

# CPUS
make_plots("cpus")

# DATA
make_plots("data")
