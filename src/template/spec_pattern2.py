import tempfile
from utils import interpret_batch, write_traces_to_dir, comp_len, treedepth, interpret, re
from mltl_parser import check_wff
import libmltl

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
    candidate_formula = None
    
    while True:
        if candidate_formula is None:
            # Generate initial candidate formulas
            candidate_formulas = generate_candidates(templates, num_propositions, time_bounds, positive_traces + negative_traces)
        else:
            # Refine candidate formula based on counterexample
            candidate_formulas = refine_formula(candidate_formula, counterexample, templates, num_propositions, time_bounds)
        
        for formula in candidate_formulas:
            if check_formula(formula, positive_traces, negative_traces):
                candidate_formula = formula
                break
        
        if candidate_formula is not None:
            # Check candidate formula against all traces
            counterexample = find_counterexample(candidate_formula, positive_traces, negative_traces)
            if counterexample is None:
                # No counterexample found, formula is consistent
                print("Generated MLTL formula:", candidate_formula)
                return candidate_formula
    
    return None

def refine_formula(formula, counterexample, templates, num_propositions, time_bounds):
    refined_formulas = []

    # Adjust time bounds
    for template in templates:
        if template in [existence_template, universality_template]:
            for prop in [f'p{i}' for i in range(num_propositions)]:
                for a, b in time_bounds:
                    if not interpret(template(prop, a, b), counterexample):
                        # Adjust time bounds based on the counterexample
                        adjusted_bounds = adjust_time_bounds(formula, a, b, counterexample)
                        for new_a, new_b in adjusted_bounds:
                            refined_formula = template(prop, new_a, new_b)
                            if not is_equivalent(refined_formula, formula, [counterexample]):
                                refined_formulas.append(refined_formula)
        else:
            for prop1 in [f'p{i}' for i in range(num_propositions)]:
                for prop2 in [f'p{i}' for i in range(num_propositions)]:
                    for a, b in time_bounds:
                        if not interpret(template(prop1, prop2, a, b), counterexample):
                            # Adjust time bounds based on the counterexample
                            adjusted_bounds = adjust_time_bounds(formula, a, b, counterexample)
                            for new_a, new_b in adjusted_bounds:
                                refined_formula = template(prop1, prop2, new_a, new_b)
                                if not is_equivalent(refined_formula, formula, [counterexample]):
                                    refined_formulas.append(refined_formula)

    # Add correct propositions
    relevant_props = identify_relevant_props(formula, counterexample)
    for prop in relevant_props:
        refined_formula = refine_with_prop(formula, prop)
        if not is_equivalent(refined_formula, formula, [counterexample]):
            refined_formulas.append(refined_formula)

    return refined_formulas

def adjust_time_bounds(formula, a, b, counterexample):
    adjusted_bounds = []
    
    # Analyze the counterexample to identify time steps where the formula fails
    violating_steps = []
    for i, (state, timestamp) in enumerate(counterexample):
        if not interpret(f'({formula})[{a},{b}]', [(state, timestamp)])[0]:
            violating_steps.append(i)
    
    # Adjust time bounds based on the violating time steps
    if violating_steps:
        min_step = min(violating_steps)
        max_step = max(violating_steps)
        adjusted_a = max(0, a - (counterexample[min_step][1] - counterexample[0][1]))
        adjusted_b = b + (counterexample[max_step][1] - counterexample[0][1])
        adjusted_bounds.append((adjusted_a, adjusted_b))
    
    return adjusted_bounds

def identify_relevant_props(formula, counterexample):
    relevant_props = []
    
    # Extract propositions from the formula
    props = re.findall(r'p\d+', formula)
    
    # Analyze the counterexample to identify relevant propositions
    for prop in props:
        prop_true_steps = [i for i, (state, _) in enumerate(counterexample) if prop in state]
        prop_false_steps = [i for i, (state, _) in enumerate(counterexample) if prop not in state]
        
        if prop_true_steps and prop_false_steps:
            # Proposition changes its truth value in the counterexample
            relevant_props.append(prop)
    
    return relevant_props

def refine_with_prop(formula, prop):
    refined_formula = formula
    
    # Refine the formula by adding or modifying propositions
    if prop not in formula:
        # Add the proposition to the formula
        refined_formula = f'({formula}) & {prop}'
    else:
        # Modify the existing proposition in the formula
        refined_formula = formula.replace(prop, f'({prop} | !{prop})')
    
    return refined_formula

def find_counterexample(formula, positive_traces, negative_traces):
    # Check formula against positive traces
    for trace in positive_traces:
        if not interpret(formula, trace):
            return trace
    
    # Check formula against negative traces
    for trace in negative_traces:
        if interpret(formula, trace):
            return trace
    
    return None

#Not using this function
def heuristic_score(formula, positive_traces, negative_traces, num_propositions):
    # Implement the heuristic function based on the literature
    # Example heuristic: Temporal operator balance
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

#Not using this function
def relevance(formula, traces):
    # Implement a function to compute the relevance of the formula to the traces
    # For example, you can count the number of traces that satisfy the formula
    count = 0
    for trace in traces:
        if interpret(formula, trace):
            count += 1
    return count / len(traces)

positive_traces, num_propositions = read_traces('dataset/basic_release/pos_summary.txt')
negative_traces, _ = read_traces('dataset/basic_release/neg_summary.txt')

time_bounds = [(0, 10), (0, 15), (5, 15)]  # Time bounds for the temporal operators
mltl_formula = synthesize_mltl(templates, num_propositions, time_bounds, positive_traces, negative_traces)

# Printing the synthesized MLTL formula
if mltl_formula:
    print("Synthesized MLTL formula:")
    print(mltl_formula)
else:
    print("No consistent MLTL formula found.")
