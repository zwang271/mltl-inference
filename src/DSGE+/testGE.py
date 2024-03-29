from GE import *
import sys
import time

def perftest_individual(params):
    print("Performance testing individual...")
    num_individuals = params["population_size"]
    intialize_time = 0
    phenotype_length = 0
    evaluate_time = 0
    formulas = set()
    for _ in tqdm(range(num_individuals)):
        # initialize individual
        start_init = time.perf_counter()
        x = Individual(params)
        end_init = time.perf_counter()
        intialize_time += end_init - start_init
        phenotype_length += len(x.phenotype)
        formulas.add(x.phenotype)
        # evaluate individual
        start_eval = time.perf_counter()
        x.evaluate()
        end_eval = time.perf_counter()
        evaluate_time += end_eval - start_eval
    print(f"Average time to initialize individual: {intialize_time/num_individuals}")
    print(f"Average length of phenotype: {phenotype_length/num_individuals}")
    print(f"Percentage of unique formulas: {len(formulas)/num_individuals*100}")
    print(f"Average time to evaluate individual: {evaluate_time/num_individuals}")

def test_mutation(params):
    # Testing Mutation
    genotype = {}
    for nt in grammar.non_terminals:
        genotype[nt] = ([random.randint(0, genotype_max) for _ in range(genotype_length)])
    genotype["<wff>"][:4] = [3, 4, 0, 1]
    x = Individual(params, genotype)
    print(x.phenotype)
    pprint(x.used_nt_count)
    while True:
        input()
        y = x.copy()
        y.mutate()
        print(y.phenotype)

def test_crossover(params):
    # Testing Crossover
    while True:
        genotype1 = {}
        genotype2 = {}
        for nt in grammar.non_terminals:
            genotype1[nt] = ([random.randint(0, genotype_max) for _ in range(genotype_length)])
            genotype2[nt] = ([random.randint(0, genotype_max) for _ in range(genotype_length)])
        genotype1["<wff>"][:4] = [3, 4, 0, 1]
        genotype2["<wff>"][:4] = [3, 4, 0, 1]
        x = Individual(params, genotype1)
        y = Individual(params, genotype2)
        print(f"{x.phenotype} | {y.phenotype}")
        z = crossover(x, y)
        print(z.phenotype, "\n")
        input()

def test_parallel_evaluate(params):
    population = initialize_population(params)
    start_parallel = time.perf_counter()
    evaluate_population(population, parallel=True)
    end_parallel = time.perf_counter()
    # print top 3 individuals
    # print("Top 3 individuals parallel")
    # for i in range(3):
    #     print(population[i])
    
    start_serial = time.perf_counter()
    evaluate_population(population, parallel=False)
    end_serial = time.perf_counter()
    # print top 3 individuals
    # print("Top 3 individuals serial")
    # for i in range(3):
    #     print(population[i])

    print(f"\nTime to evaluate population in parallel: {end_parallel - start_parallel}")
    print(f"Time to evaluate population in serial: {end_serial - start_serial}")
    print(f"Speedup: {(end_serial - start_serial)/(end_parallel - start_parallel)}")

def test_genetic_difference(params):
    # Find average pairwise genetic difference between individuals in random population
    total_genetic_difference = 0
    iterations = 10
    for _ in range(iterations):
        population = initialize_population(params)
        for i in range(len(population)):
            for j in range(i+1, len(population)):
                total_genetic_difference += genetic_difference(population[i], population[j])
    average_genetic_difference = total_genetic_difference / (iterations*len(population)*(len(population)-1)/2)
    print(f"Average pairwise genetic difference in random population: {average_genetic_difference}")

    # Find average pairwise genetic difference between individuals in population of only mutations
    total_genetic_difference = 0
    iterations = 10
    for _ in range(iterations):
        x = Individual(params)
        population = [x]
        while len(population) < params["population_size"]:
            y = x.copy()
            y.mutate()
            population.append(y)
        for i in range(len(population)):
            for j in range(i+1, len(population)):
                total_genetic_difference += genetic_difference(population[i], population[j])
    average_genetic_difference = total_genetic_difference / (iterations*len(population)*(len(population)-1)/2)
    print(f"Average pairwise genetic difference in population of only mutations: {average_genetic_difference}")



if __name__ == "__main__":
    # PARAMS
    dataset_path = "./dataset/basic/global/"
    population_size = 100
    mutation_rate = 0.2
    mutation_rate_decay = 0.95
    num_generations = 30
    max_tree_depth = 6
    temporal_nesting = 2
    fitness_weights = {
        "accuracy": 0.8,
        "treedepth": 0.05,
        "complen": 0.1,
        "length": 0.05
    }
    print_individuals = False

    # Additionally params that can be computed from the above params
    pos_train = os.path.join(dataset_path, "pos_train.txt")
    neg_train = os.path.join(dataset_path, "neg_train.txt")
    pos_test = os.path.join(dataset_path, "pos_test.txt")
    neg_test = os.path.join(dataset_path, "neg_test.txt")
    pos_train_traces = load_dataset(pos_train)
    neg_train_traces = load_dataset(neg_train)
    neg_test_traces = load_dataset(neg_test)
    pos_test_traces = load_dataset(pos_test)
    n = len(pos_train_traces[0][0])
    MAX_BOUND = max([len(trace) for trace in pos_train_traces + neg_train_traces])
    grammar = Grammar(n, MAX_BOUND)
    genotype_length = 2**(max_tree_depth+1)
    genotype_max = 1e6 
    # target complen is length of longest trace in the dataset
    target_complen = max([len(trace) for trace in pos_train_traces + neg_train_traces])

    # create params dict from parameters 
    params = {
        "dataset_path": dataset_path,
        "pos_train": pos_train,
        "neg_train": neg_train,
        "pos_test": pos_test,
        "neg_test": neg_test,
        "pos_train_traces": pos_train_traces,
        "neg_train_traces": neg_train_traces,
        "pos_test_traces": pos_test_traces,
        "neg_test_traces": neg_test_traces,
        "population_size": population_size,
        "mutation_rate": mutation_rate,
        "num_generations": num_generations,
        "max_tree_depth": max_tree_depth,
        "temporal_nesting": temporal_nesting,
        "n": n,
        "MAX_BOUND": MAX_BOUND,
        "grammar": grammar,
        "genotype_length": genotype_length,
        "genotype_max": genotype_max,
        "fitness_weights": fitness_weights,
        "print_individuals": print_individuals,
        "target_complen": target_complen,
        "mutation_rate_decay": mutation_rate_decay,
    }
    print("PARAMETERS")
    print("="*50)
    [print(f"{k}: {v}") for k, v in params.items() 
     if k not in ["grammar"] and not "traces" in k]
    print("="*50, "\n")

    test = sys.argv[1]
    if test == "perftest_individual":
        perftest_individual(params)
    elif test == "mutation":
        test_mutation(params)
    elif test == "crossover":
        test_crossover(params)
    elif test == "parallel_evaluate":
        test_parallel_evaluate(params)
    elif test == "genetic_difference":  
        test_genetic_difference(params)
    else:
        print("Invalid test")