import matplotlib.pyplot as plt

f = open("execution_times.txt", "r")

priority_times = {}

for line in f:
    words = line.split(" ")
    if len(words) == 2:
        total_time = int(words[1])
    else:
        [_, _, priority, execution_time] = words
        if priority in priority_times:
            (mean_so_far, times) = priority_times[priority]
            priority_times[priority] = ((mean_so_far * times + int(execution_time))/(times + 1), times + 1)
        else:
            priority_times[priority] = (int(execution_time), 1)

f.close()

f = open("execution_times_scheduler.txt", "r")

priority_times_scheduler = {}

for line in f:
    words = line.split(" ")
    if len(words) == 2:
        total_time_scheduler = int(words[1])
    else:
        [_, _, priority, execution_time] = words
        if priority in priority_times_scheduler:
            (mean_so_far, times) = priority_times_scheduler[priority]
            priority_times_scheduler[priority] = ((mean_so_far * times + int(execution_time))/(times + 1), times + 1)
        else:
            priority_times_scheduler[priority] = (int(execution_time), 1)
            
f.close()

x = [int(i) for i in priority_times.keys()]
x.sort()
y = [priority_times[str(i)][0] for i in x]

y_scheduler = [priority_times_scheduler[str(i)][0] for i in x]


plt.figure()
plt.plot(y, marker="x", label="without scheduler", color="limegreen")
plt.plot(y_scheduler, marker="o", label="with scheduler", color="skyblue")
plt.xticks(range(len(x)), x)

plt.title("Execution time of rpcs")
plt.ylabel("mean execution time (microseconds)")
plt.xlabel("priority")

plt.legend()

plt.savefig(f"client/go_client/plots/mean_execution_times.png", bbox_inches='tight')

plt.clf()


plt.figure()

plt.bar(["with_scheduler", "without_scheduler"], [total_time_scheduler, total_time], \
        width=0.3, color="limegreen")

plt.title("Total execution time of rpcs")
plt.ylabel("execution time (microseconds)")

plt.savefig(f"client/go_client/plots/total_execution_time.png", bbox_inches='tight')

plt.clf()
