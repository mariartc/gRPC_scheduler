import json
import os
import random
import string

# VARIABLES
PATH_TO_EXE = "../../ghz"
PATH_TO_PROTO= "../../helloworld.proto"
PATH_TO_CONFIG = "../../ghz.json"
RPC_CALL = "helloworld.Greeter.ComputeMeanRepeatedOrSendLongString"
OUTPUT_FORMAT = "pretty"
OUTPUT_FILE = " > ../../test.json"
RESULTS_FILE = "results.json"
HOST = " 127.0.0.1:50051"
OPTIONS = " --insecure --proto=" + PATH_TO_PROTO + " --call=" + RPC_CALL + \
        " --format=" + OUTPUT_FORMAT + " --config=" + PATH_TO_CONFIG + HOST + OUTPUT_FILE



# HELPER FUNCTIONS
def generate_float_number_list_data(dimension = 3, rng = 100):
    return {
                "float_number_list_type": {
                    "value": [random.randint(0, rng) for _ in range(dimension)]
                }
            }

def generate_long_string_data(length = 100):
    return {
                "long_string_type": {
                    "str": ''.join(random.choices(string.ascii_lowercase, k = length))
                }
            }

def read_results_so_far():
    file = open(RESULTS_FILE, "r")
    results_so_far = json.load(file)
    file.close()
    return results_so_far

def read_current_results():
    file = open(OUTPUT_FILE[3:], "r")
    current_results = json.load(file)
    file.close()
    return current_results

def write_ghz_json(ghz_json):
    file = open(PATH_TO_CONFIG,"w")
    file.write(json.dumps(ghz_json, indent=4))
    file.close()

def append_results(results):
    file = open(RESULTS_FILE, "w")
    file.write(json.dumps(results, indent=4))
    file.close()

def extract_information(results):
    return {
        "options": results["options"],
        "total": results["total"],
        "average": results["average"],
        "fastest": results["fastest"],
        "slowest": results["slowest"],
        "rps": results["rps"],
        "details": results["details"],
    }




# TEST CONCURRENCY
def test_concurrency(data_len_for_concurrency, data_type_for_concurrency, \
                    times_so_far, concurrency = [1, 5, 10, 50, 100, 150, 200], \
                    total_for_concurrency = 1000, cpus_for_concurrency = 2):
    ghz_json = {
        "total": total_for_concurrency,
        "cpus": cpus_for_concurrency
    }

    print(f"Testing concurrency with {total_for_concurrency} calls, {cpus_for_concurrency} cpus and {data_type_for_concurrency} data")

    results_so_far = read_results_so_far()
    if "concurrency" not in results_so_far:
        results_so_far["concurrency"] = {}

    for c in concurrency:
        # generate random data
        if data_type_for_concurrency[:23] == "float_number_list_type_":
            data_for_concurrency = [generate_float_number_list_data(data_len_for_concurrency)]
        elif data_type_for_concurrency[:17] == "long_string_type_":
            data_for_concurrency = [generate_long_string_data(data_len_for_concurrency)]
        else:
            data_for_concurrency = []
            for i in range(total_for_concurrency):
                if i % 2 == 0:
                    data_for_concurrency.append(generate_float_number_list_data(data_len_for_concurrency[0]))
                else:
                    data_for_concurrency.append(generate_long_string_data(data_len_for_concurrency[1]))

            
        # run tests
        ghz_json["concurrency"] = c
        ghz_json["data"] = data_for_concurrency
        write_ghz_json(ghz_json)
        os.system(PATH_TO_EXE+OPTIONS)

        # get results
        current_results = read_current_results()

        # append results to concurrency results
        key = f"[{total_for_concurrency}, {c}, {cpus_for_concurrency}, {data_type_for_concurrency}]"

        if key not in results_so_far["concurrency"]:
            results_so_far["concurrency"][key] = {}
        results_so_far["concurrency"][key][str(times_so_far)] = extract_information(current_results)

    #append concurrency results to results so far
    append_results(results_so_far)



# TEST TOTAL
def test_total(data_len_for_total, data_type_for_total, times_so_far, \
                total = [100, 1000, 10000, 100000], \
                concurrency_for_total = 20, \
                cpus_for_total = 2):

    ghz_json = {
        "concurrency": concurrency_for_total,
        "cpus": cpus_for_total
    }

    print(f"Testing total calls with {concurrency_for_total} concurrency, {cpus_for_total} cpus and {data_type_for_total} data")

    results_so_far = read_results_so_far()
    if "total" not in results_so_far:
        results_so_far["total"] = {}

    for n in total:
        # generate random data
        if data_type_for_total[:23] == "float_number_list_type_":
            data_for_total = [generate_float_number_list_data(data_len_for_total)]
        elif data_type_for_total[:17] == "long_string_type_":
            data_for_total = [generate_long_string_data(data_len_for_total)]
        else:
            data_for_total = []
            for i in range(n):
                if i % 2 == 0:
                    data_for_total.append(generate_float_number_list_data(data_len_for_total[0]))
                else:
                    data_for_total.append(generate_long_string_data(data_len_for_total[1]))

        # run tests
        ghz_json["total"] = n
        ghz_json["data"] = data_for_total
        write_ghz_json(ghz_json)
        os.system(PATH_TO_EXE+OPTIONS)

        # get results
        current_results = read_current_results()

        # append results to concurrency results
        key = f"[{n}, {concurrency_for_total}, {cpus_for_total}, {data_type_for_total}]"

        if key not in results_so_far["total"]:
            results_so_far["total"][key] = {}
        results_so_far["total"][key][str(times_so_far)] = extract_information(current_results)

    #append concurrency results to results so far
    append_results(results_so_far)



# TEST CPUS
def test_cpus(data_len_for_cpus, data_type_for_cpus, \
                times_so_far, cpus = [1, 2, 4, 8], \
                concurrency_for_cpus = 20, \
                total_for_cpus = 1000):

    ghz_json = {
        "concurrency": concurrency_for_cpus,
        "total": total_for_cpus
    }

    print(f"Testing cpus with {concurrency_for_cpus} concurrency, {total_for_cpus} calls and {data_type_for_cpus} data")

    results_so_far = read_results_so_far()
    if "cpus" not in results_so_far:
        results_so_far["cpus"] = {}

    for cpu in cpus:
        # generate random data
        if data_type_for_cpus[:23] == "float_number_list_type_":
            data_for_cpus = [generate_float_number_list_data(data_len_for_cpus)]
        elif data_type_for_cpus[:17] == "long_string_type_":
            data_for_cpus = [generate_long_string_data(data_len_for_cpus)]
        else:
            data_for_cpus = []
            for i in range(total_for_cpus):
                if i % 2 == 0:
                    data_for_cpus.append(generate_float_number_list_data(data_len_for_cpus[0]))
                else:
                    data_for_cpus.append(generate_long_string_data(data_len_for_cpus[1]))

        # run tests
        ghz_json["cpus"] = cpu
        ghz_json["data"] = data_for_cpus
        write_ghz_json(ghz_json)
        os.system(PATH_TO_EXE+OPTIONS)

        # get results
        current_results = read_current_results()

        # append results to cpu results
        key = f"[{total_for_cpus}, {concurrency_for_cpus}, {cpu}, {data_type_for_cpus}]"

        if key not in results_so_far["cpus"]:
            results_so_far["cpus"][key] = {}
        results_so_far["cpus"][key][str(times_so_far)] = extract_information(current_results)

    #append cpu results to results so far
    append_results(results_so_far)



# TEST DATA
def test_data(times_so_far, len_for_float_number_data_type = [10, 100, 1000, 10000], \
            len_for_long_string_data_type = [10, 100, 1000, 10000], \
            concurrency_for_data = 20, total_for_data = 1000, \
            cpus_for_data = 2):
    print(f"Testing data with {concurrency_for_data} concurrency, {total_for_data} calls and {cpus_for_data} cpus")
    data = {}

    for length in len_for_float_number_data_type:
        data[f"float_number_list_type_{length}"] = [generate_float_number_list_data(length)]
    
    for length in len_for_long_string_data_type:
        data[f"long_string_type_{length}"] = [generate_long_string_data(length)]

    ghz_json = {
        "concurrency": concurrency_for_data,
        "total": total_for_data,
        "cpus": cpus_for_data
    }

    results_so_far = read_results_so_far()
    if "data" not in results_so_far:
        results_so_far["data"] = {}

    for d_type, d in data.items():
        # run tests
        ghz_json["data"] = d
        write_ghz_json(ghz_json)
        os.system(PATH_TO_EXE+OPTIONS)

        # get results
        current_results = read_current_results()

        # append results to data results
        key = f"[{total_for_data}, {concurrency_for_data}, {cpus_for_data}, {d_type}]"

        if key not in results_so_far["data"]:
            results_so_far["data"][key] = {}
        results_so_far["data"][key][str(times_so_far)] = extract_information(current_results)

    #append data results to results so far
    append_results(results_so_far)



# TEST MULTIPLEXING
def test_multiplexing():
    data_len_float_number_list = 100
    data_len_long_string = 1000
    data_type_long_string = f"long_string_type_{data_len_long_string}"
    data_type_float_number_list = f"float_number_list_type_{data_len_float_number_list}"

    # concurrency
    for i in range(5):
        test_concurrency(data_len_float_number_list, data_type_float_number_list, i+1)
        test_concurrency(data_len_long_string, data_type_long_string, i+1)
        test_concurrency([data_len_float_number_list, data_len_long_string], "multiplexing", i+1)

    # total
    for i in range(5):
        test_total(data_len_float_number_list, data_type_float_number_list, i+1)
        test_total(data_len_long_string, data_type_long_string, i+1)
        test_total([data_len_float_number_list, data_len_long_string], "multiplexing", i+1)

    # cpus
    for i in range(5):
        test_cpus(data_len_float_number_list, data_type_float_number_list, i+1)
        test_cpus(data_len_long_string, data_type_long_string, i+1)
        test_cpus([data_len_float_number_list, data_len_long_string], "multiplexing", i+1)

# CALL FUNCTIONS
# # concurrency
# data_len_for_concurrency = 100
# data_type_for_concurrency = f"float_number_list_type_{data_len_for_concurrency}"
# for i in range(10):
#     test_concurrency(data_len_for_concurrency, data_type_for_concurrency, i+1)

# # total
# data_len_for_total = 100
# data_type_for_total = f"float_number_list_type_{data_len_for_total}"
# for i in range(10):
#     test_total(data_len_for_total, data_type_for_total, i+1)

# # cpus
# data_len_for_cpus = 100
# data_type_for_cpus = f"float_number_list_type_{data_len_for_cpus}"
# for i in range(10):
#     test_cpus(data_len_for_cpus, data_type_for_cpus, i+1)

# # data
# for i in range(10):
#     test_data(i+1)

# multiplexing
test_multiplexing()
