from lark import Lark, Transformer, v_args, exceptions
import sys
import re

# Define a MLTL grammar using EBNF notation
grammar = r'''
    num: /[0-9]+/
    prop_var: "p" num
    prop_cons: "true" | "false"
    interval: "[" num "," num "]"
    binary_prop_conn: "&" | "|" | "->"
    unary_temp_conn: F | G
    binary_temp_conn: U | R
    F: "F"
    G: "G"
    U: "U"
    R: "R"
    start : wff
    wff: prop_var | prop_cons
        | "(" wff ")"
        | "!" wff
        | "(" wff binary_prop_conn wff ")"
        | unary_temp_conn interval wff
        | "(" wff binary_temp_conn interval wff ")"
    %import common.WS
    %ignore WS
'''

parser = Lark(grammar, parser='lalr', start='wff')

def check_wff(input_string, verbose=True):
    if len(input_string) == 0:
        return False, None
    try:
        # Attempt to parse the input string
        tree = parser.parse(input_string)
        return True, tree, None
    except exceptions.LarkError as e:
        return False, None, e
