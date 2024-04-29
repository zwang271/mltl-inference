import tempfile
from utils import interpret_batch, write_traces_to_dir

# Reading the text files containing the positive and negative traces
def read_traces(file_path):
    with open(file_path, 'r') as file:
        traces = [line.strip() for line in file]
    return traces

positive_traces = read_traces('dataset/basic_future/pos_summary.txt')
negative_traces = read_traces('dataset/basic_future/neg_summary.txt')

# Instantiating the templates with propositions and time bounds
def existence_template(prop, a, b):
    return f'F[{a},{b}] {prop}'

def universality_template(prop, a, b):
    return f'G[{a},{b}] {prop}'

def until_template(prop1, prop2, a, b):
    return f'{prop1} U[{a},{b}] {prop2}'

def response_template(prop1, prop2, a, b):
    return f'G({prop1} -> F[{a},{b}] {prop2})'

templates = [existence_template, universality_template, until_template, response_template]

# Generating candidate formulas
def generate_candidates(templates, propositions, time_bounds):
    candidates = []
    for template in templates:
        if template in [existence_template, universality_template]:
            for prop in propositions:
                for a, b in time_bounds:
                    candidates.append(template(prop, a, b))
        elif template in [until_template, response_template]:
            for prop1 in propositions:
                for prop2 in propositions:
                    for a, b in time_bounds:
                        candidates.append(template(prop1, prop2, a, b))
    return candidates

propositions = ['p', 'q', 'r']  # Example propositions
time_bounds = [(0, 5), (2, 8), (1, 10)]  # Example time bounds
candidate_formulas = generate_candidates(templates, propositions, time_bounds)

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
def synthesize_mltl(templates, propositions, time_bounds, positive_traces, negative_traces):
    candidate_formulas = generate_candidates(templates, propositions, time_bounds)

    for formula in candidate_formulas:
        if check_formula(formula, positive_traces, negative_traces):
            return formula

    return None

mltl_formula = synthesize_mltl(templates, propositions, time_bounds, positive_traces, negative_traces)

# Printing the synthesized MLTL formula
if mltl_formula:
    print("Synthesized MLTL formula:")
    print(mltl_formula)
else:
    print("No consistent MLTL formula found.")
