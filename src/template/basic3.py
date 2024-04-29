import tempfile
from utils import interpret_batch, write_traces_to_dir

# Reading the text files containing the positive and negative traces
def read_traces(file_path):
    with open(file_path, 'r') as file:
        traces = [line.strip() for line in file]
    num_propositions = len(traces[0].split(',')[0])
    return traces, num_propositions

#positive_traces = read_traces('dataset/basic_future/pos_summary.txt')
#negative_traces = read_traces('dataset/basic_future/neg_summary.txt')

# Instantiating the templates with propositions and time bounds
def existence_template(prop, a, b):
    return f'F[{a},{b}] ({prop})'

def universality_template(prop, a, b):
    return f'G[{a},{b}] ({prop})'

def disjunction_template(prop1, prop2, a, b):
    return f'F[{a},{b}] (({prop1}) | ({prop2}))'

def conjunction_template(prop1, prop2, a, b):
    return f'G[{a},{b}] (({prop1}) & ({prop2}))'

def until_template(prop1, prop2, a, b):
    return f'({prop1}) U[{a},{b}] ({prop2})'

def release_template(prop1, prop2, a, b):
    return f'({prop1}) R[{a},{b}] ({prop2})'

templates = [existence_template, universality_template, disjunction_template,
             conjunction_template, until_template, release_template]

# Generating candidate formulas
def generate_candidates(templates, num_propositions, time_bounds):
    candidates = []
    propositions = [f'p{i}' for i in range(num_propositions)]
    for template in templates:
        if template in [existence_template, universality_template]:
            for prop in propositions:
                for a, b in time_bounds:
                    candidates.append(template(prop, a, b))
        else:
            for prop1 in propositions:
                for prop2 in propositions:
                    for a, b in time_bounds:
                        candidates.append(template(prop1, prop2, a, b))
    return candidates

#propositions = ['p0', 'p1']  # Update with the relevant propositions
#time_bounds = [(0, 10), (0, 15), (5, 15)]  # Update with the relevant time bounds
#candidate_formulas = generate_candidates(templates, propositions, time_bounds)

# Checking the consistency of the formulas
def check_formula(formula, positive_traces, negative_traces):
    # Write the formula to a temporary file
    with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
        f.write(formula)
        formula_path = f.name

    # Create temporary directories for positive and negative traces
    with tempfile.TemporaryDirectory() as pos_traces_dir, tempfile.TemporaryDirectory() as neg_traces_dir:
        # Write positive traces to files in the positive traces directory
        write_traces_to_dir(positive_traces, pos_traces_dir)

        # Write negative traces to files in the negative traces directory
        write_traces_to_dir(negative_traces, neg_traces_dir)

        # Run interpret_batch on positive traces
        pos_results = interpret_batch(formula_path, pos_traces_dir)

        # Run interpret_batch on negative traces
        neg_results = interpret_batch(formula_path, neg_traces_dir)

    # Check if the formula is consistent with all positive and negative traces
    return all(pos_results.values()) and not any(neg_results.values())

# Synthesizing the MLTL formula
def synthesize_mltl(templates, num_propositions, time_bounds, positive_traces, negative_traces):
    candidate_formulas = generate_candidates(templates, num_propositions, time_bounds)
    
    consistent_formulas = []
    for formula in candidate_formulas:
        if check_formula(formula, positive_traces, negative_traces):
            consistent_formulas.append(formula)
    
    if consistent_formulas:
        # Rank the consistent formulas based on complexity and relevance
        ranked_formulas = sorted(consistent_formulas, key=lambda f: (comp_len(f), -relevance(f, positive_traces)))
        return ranked_formulas[0]
    
    return None

def comp_len(formula):
    # A function to compute the complexity of the formula
    # For example, you can use the tree depth or the number of operators
    return treedepth(formula)

def relevance(formula, traces):
    # Implement a function to compute the relevance of the formula to the traces
    # For example, you can count the number of traces that satisfy the formula
    count = 0
    for trace in traces:
        if interpret(formula, trace):
            count += 1
    return count / len(traces)

positive_traces, num_propositions = read_traces('dataset/basic_global/pos_summary.txt')
negative_traces, _ = read_traces('dataset/basic_global/neg_summary.txt')

time_bounds = [(0, 10), (0, 15), (5, 15)]  # Update with the relevant time bounds
mltl_formula = synthesize_mltl(templates, num_propositions, time_bounds, positive_traces, negative_traces)

# Printing the synthesized MLTL formula
if mltl_formula:
    print("Synthesized MLTL formula:")
    print(mltl_formula)
else:
    print("No consistent MLTL formula found.")
