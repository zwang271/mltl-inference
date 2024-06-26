import re
from itertools import chain, combinations
import libmltl

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

# Checking if a formula is a valid MLTL formula
def is_valid_formula(formula):
    try:
        libmltl.parse(formula)
        return True
    except:
        return False

# Checking if two formulas are equivalent
def is_equivalent(formula1, formula2, traces):
    ast1 = libmltl.parse(formula1)
    ast2 = libmltl.parse(formula2)
    for trace in traces:
        # Convert the trace string to a list of strings
        trace_list = trace.split(',')
        result1 = ast1.evaluate(trace_list)
        result2 = ast2.evaluate(trace_list)
        if result1 != result2:
            return False
    return True

def powerset(iterable):
    s = list(iterable)
    return chain.from_iterable(combinations(s, r) for r in range(len(s) + 1))

# Generating candidate formulas from the templates
def generate_candidates(templates, abstraction, time_bounds, traces):
    candidates = []
    abstract_propositions = list(abstraction.values())
    explored_formulas = set()

    for template in templates:
        if template in [existence_template, universality_template]:
            for prop_combination in powerset(abstract_propositions):
                if prop_combination:
                    prop_formula = ' | '.join(prop_combination)
                    for a, b in time_bounds:
                        formula = template(prop_formula, a, b)
                        if is_valid_formula(formula) and not any(is_equivalent(formula, f, traces) for f in explored_formulas):
                            candidates.append(formula)
                            explored_formulas.add(formula)
        else:
            for prop1_combination in powerset(abstract_propositions):
                for prop2_combination in powerset(abstract_propositions):
                    if prop1_combination and prop2_combination:
                        prop1_formula = ' | '.join(prop1_combination)
                        prop2_formula = ' | '.join(prop2_combination)
                        for a, b in time_bounds:
                            formula = template(prop1_formula, prop2_formula, a, b)
                            if is_valid_formula(formula) and not any(is_equivalent(formula, f, traces) for f in explored_formulas):
                                candidates.append(formula)
                                explored_formulas.add(formula)

    return candidates

# Checking the consistency of the formulas
def check_formula(formula, positive_traces, negative_traces):
    ast = libmltl.parse(formula)
    
    for trace in positive_traces:
        trace_list = trace.split(',')  # Convert the trace string to a list of strings
        if not ast.evaluate(trace_list):
            return False
    
    for trace in negative_traces:
        trace_list = trace.split(',')  # Convert the trace string to a list of strings
        if ast.evaluate(trace_list):
            return False
    
    return True

# Refining the abstraction based on the counterexample
def initial_abstraction(num_propositions):
    abstraction = {}
    for i in range(num_propositions):
        abstraction[f'p{i}'] = f'p{i}'
    return abstraction

def refine_abstraction(formula, counterexample, abstraction):
    refined_abstraction = abstraction.copy()
    
    # Analyze the counterexample to identify propositions that need refinement
    for prop in identify_relevant_props(formula, counterexample):
        if prop in abstraction:
            # Refine the abstraction by splitting the proposition
            refined_abstraction[prop + '_0'] = prop
            refined_abstraction[prop + '_1'] = prop
            del refined_abstraction[prop]
    
    return refined_abstraction

# Refining the formula based on the counterexample
def adjust_time_bounds(formula, a, b, counterexample):
    adjusted_bounds = []
    
    # Analyze the counterexample to identify time steps where the formula fails
    violating_steps = []
    for i, (state, timestamp) in enumerate(counterexample):
        ast = libmltl.parse(f'({formula})[{a},{b}]')
        if not ast.evaluate([(state, timestamp)]):
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
    ast = libmltl.parse(formula)
    props = ast.get_propositions()
    
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
    
    # Refine the formula by adding missing propositions
    if prop not in formula:
        # Add the proposition to the formula using the '|' operator
        refined_formula = f'({formula}) | {prop}'
    
    return refined_formula

def refine_formula(formula, counterexample, templates, num_propositions, time_bounds):
    refined_formulas = []

    # Adjust time bounds
    for template in templates:
        if template in [existence_template, universality_template]:
            for prop in [f'p{i}' for i in range(num_propositions)]:
                for a, b in time_bounds:
                    ast = libmltl.parse(template(prop, a, b))
                    if not ast.evaluate(counterexample):
                        # Adjust time bounds based on the counterexample
                        adjusted_bounds = adjust_time_bounds(formula, a, b, counterexample)
                        for new_a, new_b in adjusted_bounds:
                            refined_formula = template(prop, new_a, new_b)
                            if is_valid_formula(refined_formula) and not is_equivalent(refined_formula, formula, [counterexample]):
                                refined_formulas.append(refined_formula)
        else:
            for prop1 in [f'p{i}' for i in range(num_propositions)]:
                for prop2 in [f'p{i}' for i in range(num_propositions)]:
                    for a, b in time_bounds:
                        ast = libmltl.parse(template(prop1, prop2, a, b))
                        if not ast.evaluate(counterexample):
                            # Adjust time bounds based on the counterexample
                            adjusted_bounds = adjust_time_bounds(formula, a, b, counterexample)
                            for new_a, new_b in adjusted_bounds:
                                refined_formula = template(prop1, prop2, new_a, new_b)
                                if is_valid_formula(refined_formula) and not is_equivalent(refined_formula, formula, [counterexample]):
                                    refined_formulas.append(refined_formula)

    # Add missing propositions based on the traces
    trace_props = set()
    for trace in counterexample:
        trace_props.update(trace.split(','))

    missing_props = trace_props - set(re.findall(r'p\d+', formula))
    for prop in missing_props:
        refined_formula = refine_with_prop(formula, prop)
        if is_valid_formula(refined_formula) and not is_equivalent(refined_formula, formula, [counterexample]):
            refined_formulas.append(refined_formula)

    return refined_formulas

# Finding a counterexample
def find_counterexample(formula, positive_traces, negative_traces):
    ast = libmltl.parse(formula)
    
    # Check formula against positive traces
    for trace in positive_traces:
        trace_list = trace.split(',')  # Convert the trace string to a list of strings
        if not ast.evaluate(trace_list):
            return trace
    
    # Check formula against negative traces
    for trace in negative_traces:
        trace_list = trace.split(',')  # Convert the trace string to a list of strings
        if ast.evaluate(trace_list):
            return trace
    
    return None

# Synthesizing the MLTL formula
def synthesize_mltl(templates, num_propositions, time_bounds, positive_traces, negative_traces):
    candidate_formula = None
    abstraction = initial_abstraction(num_propositions)
    
    while True:
        if candidate_formula is None:
            # Generate initial candidate formulas using the current abstraction
            candidate_formulas = generate_candidates(templates, abstraction, time_bounds, positive_traces + negative_traces)
        else:
            # Refine the abstraction based on the counterexample
            abstraction = refine_abstraction(candidate_formula, counterexample, abstraction)
            # Generate new candidate formulas using the refined abstraction
            candidate_formulas = generate_candidates(templates, abstraction, time_bounds, positive_traces + negative_traces)
        
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

# Main program
positive_traces, num_propositions = read_traces('dataset/basic_global/pos_summary.txt')
negative_traces, _ = read_traces('dataset/basic_global/neg_summary.txt')

time_bounds = [(0, 10), (0, 15), (5, 15)] # Time bounds for the templates
mltl_formula = synthesize_mltl(templates, num_propositions, time_bounds, positive_traces, negative_traces)

# Printing the synthesized MLTL formula
if mltl_formula:
    print("Synthesized MLTL formula:")
    print(mltl_formula)
else:
    print("No consistent MLTL formula found.")