import random

list_length = 20
priority_range = 4
number_of_calls = 500

f = open("input_file.txt", "w")

def generate_random_numbers(length, rng = 100):
    arr = [str(random.randint(0, rng)) for _ in range(length)]
    return ",".join(arr)

for _ in range(number_of_calls):
    line = "compute_mean "
    line += generate_random_numbers(list_length)
    line += " " + str(random.randint(1, priority_range)) + "\n"
    f.write(line)

f.close()
