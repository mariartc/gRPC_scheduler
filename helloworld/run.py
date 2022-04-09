import json
import os
import random
import string

# VARIABLES
PATH_TO_EXE = "C:\\Users\\marak\\Documents\\HMMY\\Thesis\\ghz-windows-x86_64\\ghz.exe"
PATH_TO_PROTO= "C:\\Users\\marak\\Documents\\HMMY\\Thesis\\gRPC\\helloworld\\helloworld.proto"
PATH_TO_CONFIG = "C:\\Users\\marak\\Documents\\HMMY\\Thesis\\gRPC\\helloworld\\ghz.json"
RPC_CALL = "helloworld.Greeter.ComputeMeanRepeatedOrSendLongString"
OUTPUT_FORMAT = "pretty"
OUTPUT_FILE = " > test.json"
RESULTS_FILE = "results.json"
HOST = " 127.0.0.1:50051"
OPTIONS = " --insecure --proto=" + PATH_TO_PROTO + " --call=" + RPC_CALL + \
        " --format=" + OUTPUT_FORMAT + " --config=" + PATH_TO_CONFIG + HOST + OUTPUT_FILE



# HELPER FUNCTIONS
def generate_float_number_list_data(dimension = 3, rng = 100):
    return random.sample(range(rng), dimension)

def generate_long_string_data(length = 100):
    return ''.join(random.choices(string.ascii_lowercase, k = length))



# CHANGE CONCURRENCY
concurrency = [1, 5, 10, 50, 100, 150, 200]
TOTAL_FOR_CONCURRENCY = 1000
CPUS_FOR_CONCURRENCY = 2
DATA_LEN_FOR_CONCURRENCY = 100
DATA_FOR_CONCURRENCY = [
                        {
                            "float_number_list_type": {
                                "value": generate_float_number_list_data(DATA_LEN_FOR_CONCURRENCY)
                            }
                        }
                    ]
DATA_TYPE_FOR_CONCURRENCY = f"float_number_list_type_{DATA_LEN_FOR_CONCURRENCY}"

ghz_json = {
    "total": TOTAL_FOR_CONCURRENCY,
    "cpus": CPUS_FOR_CONCURRENCY,
    "data": DATA_FOR_CONCURRENCY
}

print(f"Testing concurrency with {TOTAL_FOR_CONCURRENCY} calls, {CPUS_FOR_CONCURRENCY} cpus and {DATA_TYPE_FOR_CONCURRENCY} data")

file = open(RESULTS_FILE, "r")
results_so_far = json.load(file)
file.close()

concurrency_results = {}

for c in concurrency:
    # run tests
    ghz_json["concurrency"] = c
    file = open("ghz.json","w")
    file.write(json.dumps(ghz_json, indent=4))
    file.close()
    os.system(PATH_TO_EXE+OPTIONS)

    # get results
    file = open(OUTPUT_FILE[3:], "r")
    current_results = json.load(file)
    file.close()

    # append results to concurrency results
    key = f"[{TOTAL_FOR_CONCURRENCY}, {c}, {CPUS_FOR_CONCURRENCY}, {DATA_TYPE_FOR_CONCURRENCY}]"
    concurrency_results[key] = current_results

#append concurrency results to results so far
results_so_far["concurrency"] = concurrency_results
file = open(RESULTS_FILE, "w")
file.write(json.dumps(results_so_far, indent=4))
file.close()



# CHANGE TOTAL
total = [100, 10000, 100000, 500000]
CONCURRENCY_FOR_TOTAL = 20
CPUS_FOR_TOTAL = 2
DATA_LEN_FOR_TOTAL = 100
DATA_FOR_TOTAL = [
                    {
                        "float_number_list_type": {
                            "value": generate_float_number_list_data(DATA_LEN_FOR_TOTAL)
                        }
                    }
                ]
DATA_TYPE_FOR_TOTAL = f"float_number_list_type_{DATA_LEN_FOR_TOTAL}"

ghz_json = {
    "concurrency": CONCURRENCY_FOR_TOTAL,
    "cpus": CPUS_FOR_TOTAL,
    "data": DATA_FOR_TOTAL
}

print(f"Testing total calls with {CONCURRENCY_FOR_TOTAL} concurrency, {CPUS_FOR_TOTAL} cpus and {DATA_TYPE_FOR_TOTAL} data")

file = open(RESULTS_FILE, "r")
results_so_far = json.load(file)
file.close()

total_results = {}

for n in total:
    # run tests
    ghz_json["total"] = n
    file = open("ghz.json","w")
    file.write(json.dumps(ghz_json, indent=4))
    file.close()
    os.system(PATH_TO_EXE+OPTIONS)

    # get results
    file = open(OUTPUT_FILE[3:], "r")
    current_results = json.load(file)
    file.close()

    # append results to concurrency results
    key = f"[{n}, {CONCURRENCY_FOR_TOTAL}, {CPUS_FOR_TOTAL}, {DATA_TYPE_FOR_TOTAL}]"
    total_results[key] = current_results

#append concurrency results to results so far
results_so_far["total"] = total_results
file = open(RESULTS_FILE, "w")
file.write(json.dumps(results_so_far, indent=4))
file.close()



# CHANGE CPUS
cpus = [1, 2, 4, 8]
CONCURRENCY_FOR_CPUS = 20
TOTAL_FOR_CPUS = 1000
DATA_LEN_FOR_CPUS = 100
DATA_FOR_CPUS = [
                    {
                        "float_number_list_type": {
                            "value": generate_float_number_list_data(DATA_LEN_FOR_CPUS)
                        }
                    }
                ]
DATA_TYPE_FOR_CPUS = f"float_number_list_type_{DATA_LEN_FOR_CPUS}"

ghz_json = {
    "concurrency": CONCURRENCY_FOR_CPUS,
    "total": TOTAL_FOR_CPUS,
    "data": DATA_FOR_CPUS
}

print(f"Testing cpus with {CONCURRENCY_FOR_CPUS} concurrency, {TOTAL_FOR_CPUS} calls and {DATA_TYPE_FOR_CPUS} data")

file = open(RESULTS_FILE, "r")
results_so_far = json.load(file)
file.close()

cpus_results = {}

for cpu in cpus:
    # run tests
    ghz_json["cpus"] = cpu
    file = open("ghz.json","w")
    file.write(json.dumps(ghz_json, indent=4))
    file.close()
    os.system(PATH_TO_EXE+OPTIONS)

    # get results
    file = open(OUTPUT_FILE[3:], "r")
    current_results = json.load(file)
    file.close()

    # append results to cpu results
    key = f"[{TOTAL_FOR_CPUS}, {CONCURRENCY_FOR_CPUS}, {cpu}, {DATA_TYPE_FOR_CPUS}]"
    cpus_results[key] = current_results

#append cpu results to results so far
results_so_far["cpus"] = cpus_results
file = open(RESULTS_FILE, "w")
file.write(json.dumps(results_so_far, indent=4))
file.close()



# CHANGE DATA
data = {}
DATA_LEN_FOR_FLOAT_NUMBER_DATA_TYPE = 100
DATA_LEN_FOR_LONG_STRING_DATA_TYPE = 100
data[f"float_number_list_type_{DATA_LEN_FOR_FLOAT_NUMBER_DATA_TYPE}"] = [
                                    {
                                        "float_number_list_type": {
                                            "value": generate_float_number_list_data(DATA_LEN_FOR_FLOAT_NUMBER_DATA_TYPE)
                                        }
                                    }
                                ]
data[f"long_string_type_{DATA_LEN_FOR_LONG_STRING_DATA_TYPE}"] = [
                                {
                                    "long_string_type": {
                                        "str": generate_long_string_data(DATA_LEN_FOR_LONG_STRING_DATA_TYPE)
                                    }
                                }
                            ]

CONCURRENCY_FOR_DATA = 20
TOTAL_FOR_DATA = 1000
CPUS_FOR_DATA = 2

ghz_json = {
    "concurrency": CONCURRENCY_FOR_DATA,
    "total": TOTAL_FOR_DATA,
    "cpus": CPUS_FOR_DATA
}

print(f"Testing data with {CONCURRENCY_FOR_DATA} concurrency, {TOTAL_FOR_DATA} calls and {CPUS_FOR_DATA} cpus")

file = open(RESULTS_FILE, "r")
results_so_far = json.load(file)
file.close()

data_results = {}

for d_type, d in data.items():
    # run tests
    ghz_json["data"] = d
    file = open("ghz.json","w")
    file.write(json.dumps(ghz_json, indent=4))
    file.close()
    os.system(PATH_TO_EXE+OPTIONS)

    # get results
    file = open(OUTPUT_FILE[3:], "r")
    current_results = json.load(file)
    file.close()

    # append results to data results
    key = f"[{TOTAL_FOR_DATA}, {CONCURRENCY_FOR_DATA}, {CPUS_FOR_DATA}, {d_type}]"
    data_results[key] = current_results

#append data results to results so far
results_so_far["data"] = data_results
file = open(RESULTS_FILE, "w")
file.write(json.dumps(results_so_far, indent=4))
file.close()
