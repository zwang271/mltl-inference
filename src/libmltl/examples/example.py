import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)) + '/../lib')
import libmltl as mltl

# call the parse function to return an AST
ast = mltl.parse("G[0,4](~p1&p2)")

# print the parsed AST
print(ast.as_string())

# evaluate traces
trace = ["101","101","001","101","101","001"]
print(trace, ast.evaluate(trace)) # true
trace = ["101","101","001","101","111","001"]
print(trace, ast.evaluate(trace)) # false

# calculate future reach of root node
print("future reach:", ast.future_reach())

# get lower/upper bound of root node
print("lower bound:", ast.get_lower_bound())
print("upper bound:", ast.get_upper_bound())

# get operand, for binary operators use get_left/get_right
print("operand:", ast.get_operand().as_string())
# count total operators or variables in formula
print("num variables:", ast.count(mltl.Type.Variable))
# get type of a node
print("node type", ast.get_type())

# copy
copy = ast.deep_copy()

# let's modify the ast by changing the operand
new_op =  mltl.parse("(~p1&p0|F[0,2](p1))")
copy.set_operand(new_op)
print("original formula:", ast.as_string())
print("modified formula:", copy.as_string())

# build formula without parser
new_globally = mltl.Finally()
new_globally.set_lower_bound(0)
new_globally.set_upper_bound(2)
new_globally.set_operand(mltl.Variable(1)) # p1
print("newly built formula:", new_globally.as_string())

# use comparison operators: ==, !=, <, >, <=, >=
print(mltl.parse("(F[0,2](p3))") == mltl.parse("(F[0,2](p1))"))
print(mltl.parse("(F[0,2](p3))") < mltl.parse("(F[0,3](p3))"))

# error example (illegal bounds)
# ast = mltl.parse("G[11,10](~p1)")
