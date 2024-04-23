import libmltl as mltl

# Call the parse function to return an AST
ast = mltl.parse("G[0,10](~p1)")

# Print the parsed AST
print(ast.as_string())
print(ast)

# Evaluate a trace on the function
trace = ["101","100","001","101","100","001"]
print(ast.evaluate(trace)) # true
trace = ["101","100","001","101","110","001"]
print(ast.evaluate(trace)) # false

print("future reach:", ast.future_reach())

# error example (illegal bounds)
# ast = libmltl.parse("G[11,10](~p1)")

