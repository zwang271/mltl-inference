# Modular implementation of Structued Grammar Evolution 
# Author: Zili Wang

from utils import *
import mltl_parser
import random
import re
import treelib
from tqdm import tqdm
from concurrent.futures import ProcessPoolExecutor
import copy
from analyze_log import analyze_log
import numpy as np
sys.path.append(os.path.dirname(os.path.realpath(__file__)) + '/../libmltl/lib')
import libmltl as mltl

########################################################################
# Grammar Class for SGE
########################################################################
class Grammar():
    '''
    Encodes a MLTL grammar in BNF form
    '''
    def __init__(self, n, MAX_BOUND):
        '''
        Initializes a grammar object
        '''
        self.n = n
        self.MAX_BOUND = MAX_BOUND
        self.rules = self.get_grammar()
        self.non_terminals = list(self.rules.keys())

    def get_grammar(self):
        grammar = {
            "<wff>": [
                "<prop_var>",
                "!<wff>",
                "(<wff> <binary_prop_conn> <wff>)",
                "<unary_temp_conn><interval> <wff>",
                "(<wff> <binary_temp_conn><interval> <wff>)"
            ],
            "<binary_prop_conn>": ["&", "|", "->"],
            "<unary_temp_conn>": ["F", "G"],
            "<binary_temp_conn>": ["U", "R"],
            "<interval>": [f"[{i},{j}]" for i in range(self.MAX_BOUND) for j in range(i, self.MAX_BOUND)],
            "<prop_var>": [f"p{i}" for i in range(self.n)]
        }
        return grammar
    
    def terminal_expansion(self, nt, idx):
        '''
        Expands a non-terminal to a random terminal symbol
        '''
        if nt == "<wff>":
            terminals = self.rules["<prop_var>"]
            return terminals[idx % len(terminals)]
        else:
            return self.rules[nt][idx % len(self.rules[nt])]
    
    def __str__(self):
        grammar_str = ""
        for lhs in self.rules.keys():
            rhs = " | ".join(self.rules[lhs])
            grammar_str += f"{lhs}: {rhs}\n\n"
        return grammar_str

########################################################################
# Individual Class for SGE
########################################################################
class Individual():
    '''
    Encodes an individual in the population
    '''
    def __init__(self, params, genotype=None):
        '''
        Initializes an individual object
        '''
        self.params = params
        self.pos_train_size = len(params["pos_train_traces"])
        self.neg_train_size = len(params["neg_train_traces"])
        self.pos_test_size = len(params["pos_test_traces"])
        self.neg_test_size = len(params["neg_test_traces"])
        self.genotype_length = params["genotype_length"]
        self.genotype_max = params["genotype_max"]
        self.mutation_rate = params["mutation_rate"]
        self.grammar = params["grammar"]
        self.rules = self.grammar.rules  
        self.max_tree_depth = params["max_tree_depth"]
        self.fitness_weights = params["fitness_weights"]
        self.target_complen = params["target_complen"]

        # useful attributes
        self.wff_regex = re.compile(r"<wff>")
        self.nt_regex = re.compile(r"<[a-z_]+>")
        self.node_regex = re.compile(r"\d+node")
        self.used_nt_count = {nt : 0 for nt in self.grammar.non_terminals}

        # initialize attributes
        if genotype is None:
            self.genotype = self.get_genotype()
        else:   
            self.genotype = genotype
        self.phenotype_tree = self.get_phenotype_tree()
        self.phenotype = self.get_phenotype()
        self.accuracy = None
        self.treedepth = self.phenotype_tree.depth()
        self.complen = None
        self.fitness = None
        self.test_accuracy = None

    def get_genotype(self):
        '''
        Initializes the individual's genotype
        Genotype is a dictionary of lists
        One list for each non-terminal in the grammar of length genotype_length
        '''
        genotype = {}
        for nt in self.grammar.non_terminals:
            genotype[nt] = ([random.randint(0, self.genotype_max) for _ in range(self.genotype_length)])
        return genotype
    
    def get_phenotype_tree(self):
        '''
        Initializes the individual's phenotype
        '''
        idx = 0
        tree = treelib.Tree()
        tree.create_node(tag="<wff>", identifier=idx)
        root = tree.get_node(idx)
        # build tree using depth first search
        stack = [(root, 0)] # stack is a list of tuples (node, depth)
        # create a dict to store popped genes, so gene is not lost
        # initialize with empty list for each non-terminal
        popped_genes = {nt: [] for nt in self.grammar.non_terminals}
        while stack:
            node, depth = stack.pop()
            if depth >= self.max_tree_depth and node.tag == "<wff>":
                # terminal_expand <wff>
                gene = self.genotype[node.tag].pop(0)
                popped_genes[node.tag].append(gene)
                result = self.grammar.terminal_expansion("<wff>", gene)
                node.tag = result
                continue
            if node.tag == "<wff>":
                # expand non-terminal
                gene = self.genotype[node.tag].pop(0) 
                popped_genes[node.tag].append(gene)
                result = self.rules["<wff>"][gene % len(self.rules["<wff>"])]
                node.tag = result
                if "<wff>" in result:
                    # find each instance of <wff> using regex 
                    for match in self.wff_regex.finditer(result):
                        # create a new node for each instance of <wff>
                        idx += 1
                        tree.create_node(tag="<wff>", identifier=idx, parent=node.identifier)
                        stack.append((tree.get_node(idx), depth+1))
                        # replace <wff> with the new node's identifier
                        node.tag = node.tag.replace("<wff>", str(idx)+"node", 1)
            for match in self.nt_regex.finditer(node.tag):
                # expand non-terminal
                nt = match.group()
                gene = self.genotype[nt].pop(0)
                popped_genes[nt].append(gene)
                result = self.grammar.terminal_expansion(nt, gene)
                node.tag = node.tag.replace(nt, result, 1)
            # print(node.identifier, node.tag, depth)  
        # put back popped genes and update used_nt_count
        for nt in self.grammar.non_terminals:
            self.used_nt_count[nt] += len(popped_genes[nt])
            self.genotype[nt] = popped_genes[nt] + self.genotype[nt]
        self.phenotype_tree = tree
        return tree

    def get_phenotype(self):
        '''
        Initializes the individual's phenotype
        '''
        tree = self.phenotype_tree
        def node_to_str(node):
            if "node" not in node.tag:
                return node.tag
            else:
                # match every node identifier and recurse
                for match in self.node_regex.finditer(node.tag):
                    node_name = match.group()
                    node_id = int(node_name.replace("node", ""))
                    node_str = node_to_str(tree.get_node(node_id))
                    node.tag = node.tag.replace(node_name, node_str)
                return node.tag
        self.phenotype = node_to_str(tree.get_node(0))
        assert(mltl_parser.check_wff(self.phenotype)[0])
        return self.phenotype

    def evaluate(self):
        '''
        Evaluates the individual's fitness
        '''
        ast = mltl.parse(self.phenotype)
        pos_score, neg_score = 0, 0
        for trace in self.params["pos_train_traces"]:
            if ast.evaluate(trace):
                pos_score += 1
        for trace in self.params["neg_train_traces"]:
            if not ast.evaluate(trace):
                neg_score += 1
        self.accuracy = (pos_score + neg_score) / (self.pos_train_size + self.neg_train_size)
        self.treedepth = treedepth(self.phenotype)
        self.complen = comp_len(self.phenotype)
        self.fitness = self.fitness_weights["accuracy"] * self.accuracy + \
                       self.fitness_weights["complen"] * (1 - abs(self.complen - self.target_complen) / self.target_complen) + \
                       self.fitness_weights["length"] * (1 / (len(self.phenotype)+1)) + \
                       self.fitness_weights["treedepth"] * (1 / (self.treedepth+1))
        # evaluate test accuracy
        self.test()
        return self.accuracy, self.treedepth, self.complen, self.fitness

    def test(self):
        '''
        Tests the individual's accuracy on the test set
        '''
        ast = mltl.parse(self.phenotype)
        pos_score, neg_score = 0, 0
        for trace in self.params["pos_test_traces"]:
            if ast.evaluate(trace):
                pos_score += 1
        for trace in self.params["neg_test_traces"]:
            if not ast.evaluate(trace):
                neg_score += 1
        self.test_accuracy = (pos_score + neg_score) / (self.pos_test_size + self.neg_test_size)
        return self.test_accuracy

    def mutate(self):
        '''
        Mutates the individual's genotype
        '''
        for nt in self.grammar.non_terminals:
            # if nt == "<wff>":
            #     continue
            for i in range(len(self.genotype[nt])):
                if random.random() < self.mutation_rate:
                    self.genotype[nt][i] = random.randint(0, self.genotype_max)
        self.phenotype_tree = self.get_phenotype_tree()
        self.phenotype = self.get_phenotype()
        self.accuracy = None
        self.treedepth = self.phenotype_tree.depth()
        self.complen = None
        self.fitness = None
        self.test_accuracy = None

    def __str__(self):
        return f'''
        Phenotype: {self.phenotype}
        Fitness: {self.fitness}
        Training Accuracy: {self.accuracy} 
        Computation Length: {self.complen} 
        Tree Depth: {self.treedepth}
        Length: {len(self.phenotype)}
        Test Accuracy: {self.test_accuracy}
        '''

    def copy(self):
        '''
        Returns a copy of the individual
        '''
        return Individual(self.params, copy.deepcopy(self.genotype))

########################################################################
# Grammar Evolution Operations
########################################################################
def initialize_population(params):
    '''
    Initializes a unique population of individuals
    '''
    population = []
    phenotypes = []
    while len(population) < params["population_size"]:
        x = Individual(params)
        if x.phenotype not in phenotypes:
            population.append(x)
            phenotypes.append(x.phenotype)
            # print(x.phenotype)
    return population

def gendiff_sort(population):
    rest = population[1:]
    rest.sort(key=lambda x: \
                            x.fitness * (1 - params["genetic_difference"]) + \
                            params["genetic_difference"] * genetic_difference(x, population[0])
                            , reverse=True)
    population[1:] = rest
    return population

def evaluate_population(population, phenotypes=None):
    '''
    Evaluates the population
    '''
    if phenotypes is None:
        phenotypes = dict()
    for x in tqdm(population):
        x.evaluate()
        if x.phenotype not in phenotypes:
            phenotypes[x.phenotype] = x.fitness
    population.sort(key=lambda x: x.fitness, reverse=True)
    return

def selection(population):
    '''
    Selects top 20% parents from the population
    '''
    parents = population[:len(population)//5]
    return parents

def crossover(p1: Individual, p2: Individual):
    '''
    One point Crossover operator for SGE
    '''
    genotype = {}
    for nt in p1.genotype.keys():
        crossover_point = random.randint(0, p1.used_nt_count[nt])
        genotype[nt] = p1.genotype[nt][:crossover_point] + p2.genotype[nt][crossover_point:]
    offspring = Individual(p1.params, genotype)
    return offspring

def recombination(parents, target_size):
    '''
    Recombines parents to produce offspring
    '''
    offsprings = []
    while len(offsprings) < target_size:
        p1, p2 = random.sample(parents, 2)
        offspring = crossover(p1, p2)
        offsprings.append(offspring)
    return offsprings

def genetic_difference(x: Individual, y: Individual):
    '''
    Computes the genetic difference between two individuals
    Compares genotype of x and y for each non-terminal modulo the genotype length
    '''
    difference = 0
    r = 1/(len(x.grammar.non_terminals)+1)
    for nt in x.genotype.keys():
        max_used = max(x.used_nt_count[nt], y.used_nt_count[nt])
        if max_used == 0:
            continue
        num_choices = len(x.genotype[nt])
        for i in range(max_used):
            if (x.genotype[nt][i] - y.genotype[nt][i]) % num_choices != 0:
                difference += 1 * r**(i+1)
    return difference
    
########################################################################
# Grammar Evolution Algorithm
########################################################################
def GE(params, log_path):
    '''
    Structured Grammar Evolution
    '''
    # create log 
    log = open(log_path, "a")
    # initialize population
    population = initialize_population(params)
    # Create a phenotypes dictionary to keep track of unique phenotypes
    phenotypes = dict()
    # evaluate population
    log.write(f"Iteration {0} of {params['num_generations']}\n")
    log.write("Initial population:\n")
    evaluate_population(population, phenotypes)
    log.write("Number of unique phenotypes: " + str(len(phenotypes)) + "\n")
    log.write("Best individual:\n")
    log.write(str(population[0]) + "\n")
    log.write("="*50 + "\n")
    log.close()
    # get mutation rate
    mutation_rate = params["mutation_rate"]
    # Run GE
    for i in range(params["num_generations"]):
        log = open(log_path, "a")
        log.write(f"Iteration {i+1} of {params['num_generations']}\n")

        # select parents
        log.write("Selecting parents...\n")
        parents = selection(population)

        # recombine parents
        log.write("Recombining parents...\n")
        target_size = params["population_size"] - len(parents)
        offsprings = recombination(parents, target_size)

        # mutate offspring
        log.write("Mutating offspring...\n")
        for offspring in offsprings:
            while True:
                offspring.mutate()
                if offspring.phenotype not in phenotypes:
                    break
        population = parents + offsprings

        # print out all phenotypes if specified
        # if params["print_individuals"]:
        #     print("All phenotypes:")
        #     [print(x.phenotype) for x in population]
        #     log.write("All phenotypes:\n")
        #     [log.write(x.phenotype + "\n") for x in population]

        # evaluate population
        log.write("Evaluating offspring...\n")
        print(f"Evaluating offspring at iteration {i+1}...")
        evaluate_population(population, phenotypes)
        log.write("Best individual:\n")
        log.write(str(population[0]) + "\n")

        # decay mutation rate
        mutation_rate *= params["mutation_rate_decay"]
        for x in population:
            x.mutation_rate = mutation_rate
        log.write("Mutation rate: " + str(mutation_rate) + "\n")
        log.write("Number of unique phenotypes: " + str(len(phenotypes)) + "\n")
        log.write("="*50 + "\n")
        log.close()

    # return population
    return population

########################################################################
# Read Parameters
########################################################################
def read_params(file_path="params.txt"):
    '''
    Reads parameters from file and returns a dictionary of parameters
    '''
    essential_keys = ["seed",
                      "dataset_path", 
                      "population_size", 
                      "num_generations",
                      "max_tree_depth",
                      "mutation_rate", 
                      "mutation_rate_decay",    
                      "genetic_difference",
                      "temporal_nesting",
                      "accuracy",
                      "treedepth",
                      "complen",
                      "length",
                      ]
    params = {"fitness_weights": {}}
    # Read file and parse parameters
    with open(file_path, 'r') as file:
        for line in file:
            if line.startswith('#'):
                continue
            if line == '\n':
                continue
            key, value = line.split('=')
            key, value = key.strip(), value.strip()
            # Convert numeric values from string
            if key in ['population_size', 
                       'num_generations', 
                       'max_tree_depth', 
                       'temporal_nesting', 
                       'seed']:
                value = int(value)
            elif key in ['mutation_rate', 
                         'mutation_rate_decay', 
                         'genetic_difference']:
                value = float(value)
            elif key in ['accuracy',
                            'treedepth',
                            'complen',
                            'length']:
                value = float(value)
                params["fitness_weights"][key] = value 
            params[key] = value

    # Check for missing essential parameters
    for key in essential_keys:
        if key not in params:
            raise ValueError(f"Missing essential parameter: {key}")

    # Infer additional parameters
    dataset_path = params['dataset_path']
    pos_train = os.path.join(dataset_path, "pos_train/")
    neg_train = os.path.join(dataset_path, "neg_train/")
    pos_test = os.path.join(dataset_path, "pos_test/")
    neg_test = os.path.join(dataset_path, "neg_test/")

    # Assuming load_dataset and Grammar are defined elsewhere
    pos_train_traces = load_traces(pos_train)
    neg_train_traces = load_traces(neg_train)
    pos_test_traces = load_traces(pos_test)
    neg_test_traces = load_traces(neg_test)

    n = len(pos_train_traces[0][0])
    print(f"n = {n}")
    MAX_BOUND = max([len(trace) for trace in pos_train_traces + neg_train_traces])
    MIN_BOUND = min([len(trace) for trace in pos_train_traces + neg_train_traces])
    grammar = Grammar(n, MAX_BOUND)
    genotype_length = 2 ** (params['max_tree_depth'] + 1)
    genotype_max = 1e6
    target_complen = MIN_BOUND

    params.update({
        "pos_train": pos_train,
        "neg_train": neg_train,
        "pos_test": pos_test,
        "neg_test": neg_test,
        "pos_train_traces": pos_train_traces,
        "neg_train_traces": neg_train_traces,
        "pos_test_traces": pos_test_traces,
        "neg_test_traces": neg_test_traces,
        "n": n,
        "MAX_BOUND": MAX_BOUND,
        "grammar": grammar,
        "genotype_length": genotype_length,
        "genotype_max": genotype_max,
        "target_complen": target_complen
    })

    params.update({
        "pos_train": pos_train,
        "neg_train": neg_train,
        "pos_test": pos_test,
        "neg_test": neg_test,
        # ... (other additional parameters)
    })

    log_path = params["log_path"]
    with open (log_path, "w") as log:
        print("PARAMETERS")
        log.write("PARAMETERS\n")
        print("="*50)
        log.write("="*50 + "\n")
        [print(f"{k}: {v}") for k, v in params.items() 
        if k not in ["grammar"] and not "traces" in k]
        [log.write(f"{k}: {v}\n") for k, v in params.items()
        if k not in ["grammar"] and not "traces" in k]
        print("="*50, "\n")
        log.write("="*50 + "\n\n")
    return params

if __name__ == "__main__":
    if len(sys.argv) > 1:
        params_path = sys.argv[1]
    else:
        print("No parameters file provided. Using ./params.txt")
        params_path = "params.txt"
    params = read_params(params_path)
    log_path = params["log_path"]
    # use seed if provided 
    if params["seed"] is not None:
        random.seed(params["seed"])
        np.random.seed(params["seed"])

    # run GE
    print("Running GE...")
    print(f"Track progress in {log_path}")
    population = GE(params, log_path)
    print("Top 5 best individuals")
    print("="*50)
    [print(x) for x in population[:5]]
    # write to log
    with open (log_path, "a") as log:
        log.write("All individuals\n")
        log.write("="*50 + "\n")
        [log.write(str(x) + "\n") for x in population]

    # analyze logs and plot
    # analyze_log(log_path)