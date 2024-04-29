import tempfile
from utils import interpret_batch, write_traces_to_dir, comp_len, treedepth, interpret, re
from mltl_parser import check_wff

# Checking if a formula is a valid MLTL formula
def is_valid_formula(formula):
    valid, _, _ = check_wff(formula)
    return valid

# Reading the text files containing the positive and negative traces
def read_traces(file_path):
    with open(file_path, 'r') as file:
        traces = [line.strip() for line in file]
    num_propositions = len(traces[0].split(',')[0])
    return traces, num_propositions

# Instantiating the templates with propositions and time bounds
def existence_template(prop, a, b):
    return f'F[{a},{b}] {prop}'

def universality_template(prop, a, b):
    return f'G[{a},{b}] {prop}'

def disjunction_template(prop1, prop2, a, b):
    return f'F[{a},{b}] ({prop1} | {prop2})'

def conjunction_template(prop1, prop2, a, b):
    return f'G[{a},{b}] ({prop1} & {prop2})'

def until_template(prop1, prop2, a, b):
    return f'({prop1} U[{a},{b}] {prop2})'

def release_template(prop1, prop2, a, b):
    return f'({prop1} R[{a},{b}] {prop2})'

templates = [existence_template, universality_template, disjunction_template,
             conjunction_template, until_template, release_template]

# Checking if two formulas are equivalent
def is_equivalent(formula1, formula2, traces):
    for trace in traces:
        result1 = interpret(formula1, trace)
        result2 = interpret(formula2, trace)
        if result1 != result2:
            return False
    return True

# Generating candidate formulas from the templates
def generate_candidates(templates, num_propositions, time_bounds, traces):
    candidates = []
    propositions = [f'p{i}' for i in range(num_propositions)]
    explored_formulas = set()

    for template in templates:
        if template in [existence_template, universality_template]:
            for prop in propositions:
                for a, b in time_bounds:
                    formula = template(prop, a, b)
                    if not any(is_equivalent(formula, f, traces) for f in explored_formulas):
                        candidates.append(formula)
                        explored_formulas.add(formula)
        else:
            for prop1 in propositions:
                for prop2 in propositions:
                    for a, b in time_bounds:
                        formula = template(prop1, prop2, a, b)
                        if not any(is_equivalent(formula, f, traces) for f in explored_formulas):
                            candidates.append(formula)
                            explored_formulas.add(formula)

    return candidates

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
    candidate_formulas = generate_candidates(templates, num_propositions, time_bounds, positive_traces + negative_traces)
    
    consistent_formulas = []
    for formula in candidate_formulas:
        try:
            if check_formula(formula, positive_traces, negative_traces):
                consistent_formulas.append(formula)
        except Exception as e:
            print(f"Error occurred while checking formula: {formula}")
            print(f"Error message: {str(e)}")
    
    if consistent_formulas:
        try:
            # Compute heuristic scores for consistent formulas
            heuristic_scores = [heuristic_score(f, positive_traces, negative_traces, num_propositions) for f in consistent_formulas]
           
            # Rank formulas based on heuristic scores and other metrics
            ranked_formulas = sorted(zip(consistent_formulas, heuristic_scores), 
                                     key=lambda x: (x[1], comp_len(x[0]), treedepth(x[0]), -relevance(x[0], positive_traces)), 
                                     reverse=True)
            
            best_formula = ranked_formulas[0][0]
            print("Generated MLTL formula:", best_formula)
            return best_formula
        except Exception as e:
            print("Error occurred while ranking formulas:")
            print(f"Error message: {str(e)}")
    
    return None

def heuristic_score(formula, positive_traces, negative_traces, num_propositions):
    temporal_operators = re.findall(r'[FGUR]', formula)
    operator_counts = {op: temporal_operators.count(op) for op in set(temporal_operators)}
    if len(temporal_operators) > 0:
        balance_score = 1 - (max(operator_counts.values()) - min(operator_counts.values())) / len(temporal_operators)
    else:
        balance_score = 1
    
    # Example heuristic: Unique variable usage
    variables = re.findall(r'p\d+', formula)
    unique_variables = set(variables)
    variable_score = len(unique_variables) / num_propositions
    
    # Combine heuristic scores
    heuristic_score = balance_score + variable_score
    
    return heuristic_score

def relevance(formula, traces):
    count = 0
    for trace in traces:
        if interpret(formula, trace):
            count += 1
    return count / len(traces)

positive_traces, num_propositions = read_traces('dataset/basic_release/pos_summary.txt')
negative_traces, _ = read_traces('dataset/basic_release/neg_summary.txt')

time_bounds = [(0, 10), (0, 15), (5, 15)]
mltl_formula = synthesize_mltl(templates, num_propositions, time_bounds, positive_traces, negative_traces)

# Printing the synthesized MLTL formula
if mltl_formula:
    print("Synthesized MLTL formula:")
    print(mltl_formula)
else:
    print("No consistent MLTL formula found.")
