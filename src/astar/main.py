import libmltl as mltl

# Call the parse function to return an AST
ast = mltl.parse("G[0,10](~p1)")

# Print the parsed AST
print(ast.as_string())

# Evaluate a trace on the function
trace = ["101","100","001","101","100","001"]
print(ast.evaluate(trace)) # true
trace = ["101","100","001","101","110","001"]
print(ast.evaluate(trace)) # false

print("future reach:", ast.future_reach())

# print(ast.get_operand().as_string())
print(ast.get_upper_bound())
print(ast.get_operand().as_string())
print(ast.count(mltl.Type.Globally))
print(ast.get_type())

new_op =  mltl.parse("(~p1&p0)")
ast.set_operand(new_op)
print(ast.as_string())

new_globally = mltl.Globally()
new_globally.set_lower_bound(0)
new_globally.set_upper_bound(10)
new_globally.set_operand(mltl.Variable(1))
print(new_globally.as_string())

copy = new_globally.deep_copy()
print(copy.as_string())


# print(ast.release_operand().as_string())

# error example (illegal bounds)
ast = mltl.parse("G[11,10](~p1)")

