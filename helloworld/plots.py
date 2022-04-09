import matplotlib.pyplot as plt
import json


with open('results.json') as json_file:
    results = json.load(json_file)

def extract_details(string):
    string = string.replace("[", "")
    string = string.replace("]", "")
    return string.split(", ")

# CONCURRENCY
concurrency_keys = list(results["concurrency"].keys())
[total, _, cpus, data_type] = extract_details(concurrency_keys[0])
concurrencies = [extract_details(x)[1] for x in concurrency_keys]
average_latency = [results["concurrency"][x]["average"] for x in concurrency_keys]

plt.figure()
plt.title(f"Average latency for {total} calls, {cpus} cpus and {data_type}")
plt.plot(average_latency, color="limegreen")
plt.ylabel('Average latency') # TODO: add measure unit
plt.xlabel('Concurrency')
plt.xticks(range(len(concurrencies)), concurrencies)
plt.savefig('plots/average_latency_concurrency.png', bbox_inches='tight')

plt.clf()

# TOTAL
total_keys = list(results["total"].keys())
[_, concurrency, cpus, data_type] = extract_details(total_keys[0])
total = [extract_details(x)[0] for x in total_keys]
average_latency = [results["total"][x]["average"] for x in total_keys]

plt.figure()
plt.title(f"Average latency for {concurrency} concurrency, {cpus} cpus and {data_type}")
plt.plot(average_latency, color="limegreen")
plt.ylabel('Average latency') # TODO: add measure unit
plt.xlabel('Total calls')
plt.xticks(range(len(total)), total)
plt.savefig('plots/average_latency_total.png', bbox_inches='tight')

plt.clf()

# CPUS
cpus_keys = list(results["cpus"].keys())
[total, concurrency, _, data_type] = extract_details(cpus_keys[0])
cpus = [extract_details(x)[2] for x in cpus_keys]
average_latency = [results["cpus"][x]["average"] for x in cpus_keys]

plt.figure()
plt.title(f"Average latency for {concurrency} concurrency, {total} calls and {data_type}")
plt.plot(average_latency, color="limegreen")
plt.ylabel('Average latency') # TODO: add measure unit
plt.xlabel('Number of cpu cores')
plt.xticks(range(len(cpus)), cpus)
plt.savefig('plots/average_latency_cpus.png', bbox_inches='tight')

plt.clf()

# DATA
data_keys = list(results["data"].keys())
[total, concurrency, cpus, _] = extract_details(data_keys[0])
data = [extract_details(x)[3] for x in data_keys]
average_latency = [results["data"][x]["average"] for x in data_keys]

plt.figure()
plt.title(f"Average latency for {concurrency} concurrency, {total} calls and {cpus} cpus")
plt.bar(data, average_latency, color="limegreen", width = 0.4)
plt.ylabel('Average latency') # TODO: add measure unit
plt.xlabel('Data type')
plt.savefig('plots/average_latency_data.png', bbox_inches='tight')

plt.clf()
