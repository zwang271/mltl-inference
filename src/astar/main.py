import libmltl

# Call the parse function to return an AST
ast = libmltl.parse("G[0,10](~p1)")

# Print the parsed AST
print(ast.as_string())

# Evaluate a trace on the function
trace = ["101","100","001","101","100","001"]
print(ast.evaluate(trace)) # true
trace = ["101","100","001","101","110","001"]
print(ast.evaluate(trace)) # false
