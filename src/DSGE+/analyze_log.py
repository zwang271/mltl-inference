import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')
import os
import re
import sys

def extract_data_from_log(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()
    data = {
        'iteration': [],
        'fitness': [],
        'training_accuracy': [],
        'test_accuracy': [],
        'computation_length': [],
        'phenotype': [],
        'unique_phenotypes': [],
        'tree_depth': [],
        'formula_length': [],
        'target_complen': None,
    }
    DATASET = None

    for line in lines:
        # Extract DATASET path
        if 'dataset_path:' in line:
            DATASET = line.split(':')[1].strip()
            data['DATASET'] = DATASET

        # Extract iteration
        iteration_match = re.search(r'Iteration (\d+) of \d+', line)
        if iteration_match:
            data['iteration'].append(int(iteration_match.group(1)))

        # Extract fitness
        if 'Fitness:' in line:
            fitness = float(line.split(':')[1].strip())
            data['fitness'].append(fitness)

        # Extract accuracy
        if 'Training Accuracy:' in line:
            accuracy = float(line.split(':')[1].strip())
            data['training_accuracy'].append(accuracy)

        # Extract test accuracy
        if 'Test Accuracy:' in line:
            test_accuracy = float(line.split(':')[1].strip())
            data['test_accuracy'].append(test_accuracy)

        # Extract computation length
        if 'Computation Length:' in line:
            computation_length = int(line.split(':')[1].strip())
            data['computation_length'].append(computation_length)

        # Extract phenotype
        if 'Phenotype:' in line:
            phenotype = line.split(':', 1)[1].strip()
            data['phenotype'].append(phenotype)

        # Extract number of unique phenotypes
        if 'Number of unique phenotypes:' in line:
            unique_phenotypes = int(line.split(':')[1].strip())
            data['unique_phenotypes'].append(unique_phenotypes)

        # Extract tree depth
        if 'Tree Depth:' in line:
            tree_depth = int(line.split(':')[1].strip())
            data['tree_depth'].append(tree_depth)
        
        # Extract formula length
        if 'Length:' in line and 'Computation' not in line:
            formula_length = int(line.split(':')[1].strip())
            data['formula_length'].append(formula_length)

        # Extract target computation length
        if 'target_complen:' in line:
            target_complen = int(line.split(':')[1].strip())
            data['target_complen'] = target_complen
    return data

def get_target_formula(DATASET):
    with open(os.path.join(DATASET,"metadata.txt"), 'r') as file:
        # read the first line
        formula = file.readline().replace("formula: ", "")
    return formula.strip()

def analyze_log(log_path):
    data = extract_data_from_log(log_path) 
    target_formula = get_target_formula(data["DATASET"]) 
    # print lengths of data
    iterations = len(data['iteration'])
    for key in data:
        if not isinstance(data[key], list):
            continue
        # truncate data to the length of iterations
        data[key] = data[key][:iterations]
        print(f'{key}: {len(data[key])}')

    # Create several plots side by side, horizontally
    fig, axs = plt.subplots(2, 3, figsize=(20,12))
    fig.suptitle(f"Target Formula: {target_formula}\n Best Phenotype: {data['phenotype'][-1]}")
    # Plot 1: Fitness vs. Iteration
    axs[0, 0].plot(data['iteration'], data['fitness'])
    axs[0, 0].set_xlabel('Iteration')
    axs[0, 0].set_ylabel('Fitness')
    axs[0, 0].set_title('Fitness vs. Iteration')

    # Plot 2: Training/Test Accuracy vs. Iteration
    axs[0, 1].plot(data['iteration'], data['training_accuracy'], label='Training Accuracy')
    axs[0, 1].plot(data['iteration'], data['test_accuracy'], label='Test Accuracy')
    axs[0, 1].set_xlabel('Iteration')
    axs[0, 1].set_ylabel('Accuracy')
    axs[0, 1].set_title('Accuracy vs. Iteration')
    axs[0, 1].legend()

    # Plot 1 and 2 share the same y-axis scale
    min_y = min(min(data['fitness']), min(data['training_accuracy']), min(data['test_accuracy']))
    max_y = max(max(data['fitness']), max(data['training_accuracy']), max(data['test_accuracy']))
    buffer = (max_y - min_y) * 0.1
    min_y -= buffer
    max_y += buffer
    axs[0, 0].set_ylim(min_y, max_y)
    axs[0, 1].set_ylim(min_y, max_y)

    # Plot 3: Computation Length vs. Iteration
    axs[0, 2].plot(data['iteration'], data['computation_length'])
    # dashed horizontal line for target computation length
    axs[0, 2].axhline(y=data['target_complen'], color='r', linestyle='--', label='Target Computation Length')
    axs[0, 2].legend()
    axs[0, 2].set_xlabel('Iteration')
    axs[0, 2].set_ylabel('Computation Length')
    axs[0, 2].set_title('Computation Length vs. Iteration')

    # Plot 4: Formula Length vs. Iteration
    axs[1, 0].plot(data['iteration'], data['formula_length'])
    axs[1, 0].set_xlabel('Iteration')
    axs[1, 0].set_ylabel('Formula Length')
    axs[1, 0].set_title('Formula Length vs. Iteration')

    # Plot 5: Tree Depth vs. Iteration
    axs[1, 1].plot(data['iteration'], data['tree_depth'])
    axs[1, 1].set_xlabel('Iteration')
    axs[1, 1].set_ylabel('Tree Depth')
    axs[1, 1].set_title('Tree Depth vs. Iteration')

    # Plot 5: List at which iteration the phenotype changed
    axs[1, 2].axis('off')
    axs[1, 2].text(0, 1, 'Phenotype Changes', fontsize=12, fontweight='bold')
    last_unique_index = 0
    count = 0 
    change_indices = [0]
    for i, phenotype in enumerate(data['phenotype']):
        if i == 0:
            axs[1, 2].text(0, 0.9 - (i * 0.05), f'Iteration {i}: {phenotype}')
            continue
        if phenotype != data['phenotype'][last_unique_index]:
            count += 1
            axs[1, 2].text(0, 0.9 - (count * 0.05), f'Iteration {i}: {phenotype}')
            last_unique_index = i
            change_indices.append(i)
    
    save_path = f'{log_path.replace(".log", "")}.png'
    plt.savefig(save_path)
    print(f"Saved plot to {save_path}")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        log_path = sys.argv[1]
    else:
        log_path = input("Enter log file path: ")
    analyze_log(log_path)
    