import os, sys
import re
sys.path.append(os.path.dirname(os.path.realpath(__file__)) + '/../libmltl/lib')
import libmltl as mltl

# call the parse function to return an AST
ast = mltl.parse("G[0,4](~p1&p2)")

print(ast.as_pretty_string())
print(ast.size())

# error example (illegal bounds)
# ast = mltl.parse("G[11,10](~p1)")


current_dir = os.path.dirname(os.path.abspath(__file__))
directories = [d for d in os.listdir(current_dir) if os.path.isdir(os.path.join(current_dir, d))]

for directory in directories:
    formula_file_path = os.path.join(current_dir, directory, 'formula.txt')
    with open(formula_file_path, 'r') as formula_file:
        formula_txt = formula_file.readline()
        pattern = r'a(\d+)'
        # Replace 'a' followed by digits with 'p#'
        formula_txt = re.sub(pattern, lambda match: 'p' + match.group(1), formula_txt)
        ast = mltl.parse(formula_txt)
        formula_as_string = ast.as_pretty_string()
        size = ast.size()

        metadata_file_path = os.path.join(current_dir, directory, 'metadata.txt')
        with open(metadata_file_path, 'r') as metadata_file:
            for line in metadata_file:
                if line.startswith('n:'):
                    num_vars = line.split(':')[1].strip()
                    break

        print(f"{directory}\t{formula_as_string}\t{size}\t{num_vars}")
        overall_avg_trace_len = 0

        subdirectories = ['neg_test', 'neg_train', 'pos_test', 'pos_train']
        for subdirectory in subdirectories:
            subdirectory_path = os.path.join(current_dir, directory, subdirectory)
            files = [f for f in os.listdir(subdirectory_path) if os.path.isfile(os.path.join(subdirectory_path, f))]

            total_lines = 0
            total_files = 0
            for file_name in files:
                file_path = os.path.join(subdirectory_path, file_name)
                with open(file_path, 'r') as file:
                    lines = file.readlines()
                    total_lines += len(lines)
                    total_files += 1

            average_trace_len = total_lines / total_files
            overall_avg_trace_len = overall_avg_trace_len + average_trace_len

            print(f"  {subdirectory}\t{average_trace_len}\t{len(files)}\t")
        overall_avg_trace_len = overall_avg_trace_len / 4
        print(f"  avg trace len: {overall_avg_trace_len}")
