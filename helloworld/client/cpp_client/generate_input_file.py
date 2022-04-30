import random

f = open("input_file.txt", "w")

def generate_random_numbers(length, rng = 100):
    arr = [str(random.randint(0, rng)) for _ in range(length)]
    return ",".join(arr)

for _ in range(100):
    line = "compute_mean "
    line += generate_random_numbers(20)
    line += " " + str(random.randint(1, 10)) + "\n"
    f.write(line)

f.close()
