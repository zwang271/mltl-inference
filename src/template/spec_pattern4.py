import re
from itertools import chain, combinations
from collections import defaultdict
import libmltl
from sklearn.model_selection import train_test_split
import time

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

# Adding some new templates to the list for testing (Work in progress)

def nested_until_template(prop1, prop2, prop3, a, b, c, d):
    return f'F[{a},{b}] ({prop1} U[{c},{d}] {prop2} U {prop3})'

def nested_release_template(prop1, prop2, prop3, a, b, c, d):
    return f'G[{a},{b}] ({prop1} R[{c},{d}] {prop2} R {prop3})'

def conj_temporal_template(prop1, prop2, a, b, c, d):
    return f'({existence_template(prop1, a, b)} & {universality_template(prop2, c, d)})'

def disj_temporal_template(prop1, prop2, a, b, c, d):
    return f'({existence_template(prop1, a, b)} | {universality_template(prop2, c, d)})'

def conditional_existence_template(prop1, prop2, a, b):
    return f'({prop1} -> F[{a},{b}] {prop2})'

def conditional_universality_template(prop1, prop2, a, b):
    return f'(G[{a},{b}] {prop1} -> F {prop2})'

def periodic_template(prop1, prop2, k):
    return f'G ({prop1} -> X[{k}] {prop2})'

def response_template(prop1, prop2, a, b):
    return f'G ({prop1} -> F[{a},{b}] {prop2})'

def response_until_template(prop1, prop2, prop3, a, b, c, d):
    return f'G ({prop1} -> ({prop2} U[{c},{d}] {prop3}))'

def precedence_template(prop1, prop2, a, b):
    return f'G ({prop2} -> P[{a},{b}] {prop1})'

def precedence_since_template(prop1, prop2, prop3, a, b, c, d):
    return f'G ({prop2} -> ({prop1} S[{c},{d}] {prop3}))'

def fairness_template(prop, a, b):
    return f'GF[{a},{b}] {prop}'

def conj_fairness_template(prop1, prop2, a, b, c, d):
    return f'GF[{a},{b}] ({prop1} & GF[{c},{d}] {prop2})'


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

from collections import defaultdict

def compute_consistency_score(formula, positive_traces, negative_traces):
    total_traces = len(positive_traces) + len(negative_traces)
    consistent_traces = 0

    for trace in positive_traces:
        if check_formula(formula, [trace], negative_traces):
            consistent_traces += 1

    for trace in negative_traces:
        if not check_formula(formula, positive_traces, [trace]):
            consistent_traces += 1

    return consistent_traces / total_traces

def rank_formulas(formulas, positive_traces, negative_traces):
    ranked_formulas = defaultdict(list)

    for formula in formulas:
        score = compute_consistency_score(formula, positive_traces, negative_traces)
        ranked_formulas[score].append(formula)

    return sorted(ranked_formulas.items(), reverse=True)

def prune_low_ranked_formulas(ranked_formulas, threshold):
    pruned_formulas = []
    for score, formulas in ranked_formulas:
        if score >= threshold:
            pruned_formulas.extend(formulas)
        else:
            break
    return pruned_formulas

def generate_proposition_joins(propositions):
    proposition_joins = []
    for prop_combination in powerset(propositions):
        if prop_combination:
            join_formula = ' & '.join(prop_combination)
            proposition_joins.append(join_formula)
    return proposition_joins

def generate_candidates(templates, num_propositions, time_bounds, traces, abstraction, depth=0, max_depth=2): # Maximum depth for the search <----------------------------------------------------------
    candidates = []
    propositions = [f'p{i}' for i in range(num_propositions)]
    explored_formulas = set()

    # Rank formulas by consistency score
    ranked_formulas = rank_formulas(explored_formulas, positive_traces, negative_traces)

    # Prune low-ranked formulas
    pruned_formulas = prune_low_ranked_formulas(ranked_formulas, threshold=0.8)

    # Generate proposition joins
    proposition_joins = generate_proposition_joins(propositions)

    if depth <= max_depth:
        for formula in pruned_formulas:
            # Explore refinements of the highest-ranked formulas first
            sub_candidates = refine_formula(formula, counterexample, templates, num_propositions, time_bounds)
            candidates.extend(sub_candidates)

            # Add the formula itself to the candidates
            candidates.append(formula)

        for template in templates:
            if template in [existence_template, universality_template]:
                for prop_formula in proposition_joins:
                    for a, b in time_bounds:
                        formula = template(prop_formula, a, b)
                        if is_valid_formula(formula) and not any(is_equivalent(formula, f, traces) for f in explored_formulas):
                            candidates.append(formula)
                            explored_formulas.add(formula)
            else:
                for prop1_formula in proposition_joins:
                    for prop2_formula in proposition_joins:
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
        trace_list = trace.split(',')
        if not ast.evaluate(trace_list):
            return trace
    
    # Check formula against negative traces
    for trace in negative_traces:
        trace_list = trace.split(',')
        if ast.evaluate(trace_list):
            return trace
    
    return None

# Synthesizing the MLTL formula
def synthesize_mltl(templates, num_propositions, time_bounds, positive_traces, negative_traces, max_depth=2):
    candidate_formula = None
    abstraction = initial_abstraction(num_propositions)
    depth = 0
    
    while True:
        if candidate_formula is None:
            # Generate initial candidate formulas using the current abstraction
            candidate_formulas = generate_candidates(templates, num_propositions, time_bounds, positive_traces + negative_traces, abstraction, depth, max_depth)
        else:
            # Refine the abstraction based on the counterexample
            abstraction = refine_abstraction(candidate_formula, counterexample, abstraction)
            depth += 1
            # Generate new candidate formulas using the refined abstraction
            candidate_formulas = generate_candidates(templates, num_propositions, time_bounds, positive_traces + negative_traces, abstraction, depth, max_depth)
        
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
#basic_future
#basic_global
#basic_release
#basic_until
#rv14_formula1
#rv14_formula2
positive_traces, num_propositions = read_traces('dataset/rv14_formula1/pos_summary.txt') # Path for positive traces <----------------------------------------------------------
negative_traces, _ = read_traces('dataset/rv14_formula1/neg_summary.txt') # Path for negative traces <----------------------------------------------------------

# Split the data into training and testing sets
train_size = 0.8 # Size for the training set <----------------------------------------------------------

pos_train, pos_test = train_test_split(positive_traces, train_size=train_size, random_state=42)
neg_train, neg_test = train_test_split(negative_traces, train_size=train_size, random_state=42)

time_bounds = [(0, 10), (0, 15), (5, 15)]  # Time bounds for the templates <----------------------------------------------------------

start_time = time.time()
mltl_formula = synthesize_mltl(templates, num_propositions, time_bounds, pos_train, neg_train)
end_time = time.time()

# Calculate accuracy on the testing set
if mltl_formula:
    total_traces = len(pos_test) + len(neg_test)
    correct_predictions = 0

    for test_set in [pos_test, neg_test]:
        for trace in test_set:
            if test_set is pos_test:
                if check_formula(mltl_formula, [trace], []):
                    correct_predictions += 1
            else:
                if check_formula(mltl_formula, [], [trace]):
                    correct_predictions += 1

    accuracy = correct_predictions / total_traces
    print(f"Accuracy on testing set: {accuracy:.4f}")
else:
    print("Cannot calculate accuracy as no consistent MLTL formula was found.")

# Calculate runtime
runtime = end_time - start_time
print(f"Runtime: {runtime:.4f} seconds")