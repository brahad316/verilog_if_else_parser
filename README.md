# Parser for verilog if-else block implemented in C and Verilog

## Overview
This repository contains a Verilog implementation of a state-driven Deterministic Finite Automaton (DFA) designed to parse if-else statements. This approach accepts the input as a stream of ASCII characters, processes and evaluates the condition an then does the appropriate assignment. It also raises appropriate flags for different types of errors.

## Input file structure
The input file must be a verilog (`.v`) file of the follwoing format:
```
if(x_var == valC)
    // true branch
    begin 
        p_var <= const1;
    end
else
    // false branch
    begin
        p_var <= const2;
    end
```
Where `valC`, `const1` and `const2` are of data type integer. Any whitespace characters will be treated the way iverilog does.

The second input will be the user input, the corresponding script will prompt for it.
Note that:
- The identifiers in both the true and false branches of the if-else statement must be same, otherwise the parser throws an error.
- The variable names can be any legal verilog name: [a-zA-Z][a-zA-Z0-9_]*, upto 15 characters long (can be tweaked).
- It supports both `<=` and `=` as the assignment operators.
- It supports integer values for the constants, thus negative values too.
- It supports all the standard verilog comparators (`==`, `!=`, `<=`, `>=`, `<`, `>`).
- Takes care of all whitespace characters (whitespace, tabs, newline char, etc.).
- Flags bad syntax and incorrect parentheses.

## Parser in C

`carser.c`: This parser written in C reads from an input verilog file `input.v` and parses the content to store the relevant constant values, 
evaluate the condition and do the respective assignment based on the evaluation result. The script will prompt the user for their value of `x`.

To compile and run this, do:

```
gcc carser.c -o carser && ./carser
```

## Verilog interpreter

The scripts `c_parser_2.c` and `if_else_parser_2.v` contribute to this task. `c_parser_2.c` is a pre-processor which reads the input file `input.v` 
and organizes the relevant contents in a useful manner, it also creates the test bench (will be saved as `if_else_parser_tb_gen.v`) 
for our verilog interpreter circuit described by `if_else_parser_2.v`. The generated test bench feeds the input character by character (7 bit ASCII)
to the interpreter. It also feeds in the user input value for `x`.

`if_else_parser_2.v` is a circuit that takes in 7 bit ASCII characters, one at a time and performs the task of evaluating our if-else block followed
by doing the necessary variable assignment. 

To compile and run this, do:

```
gcc c_parser_2.c -o c_parser && ./c_parser
```
```
iverilog -o parser_gen if_else_parser_2.v if_else_parser_tb_gen.v && vvp ./parser_gen
```
## FSM overview

The latest solution, in `if_else_parser_2.v` runs a 25 state FSM (DFA). The states are as follows:
```
IDLE                      = 0,
READ_IF                   = 1,  // Concludes reading the "if" keyword
READ_OPEN_PAREN           = 2,  // Reads "("
READ_VAR                  = 3,  // Reads the condition variable (x_var)
READ_COND_OPERATOR        = 4,  // Reads the first char of the comparator (<, >, ! or =)
READ_COND_OPERATOR2       = 5,  // Reads the second char of the comparator if any (for <=, >=, != or ==)
READ_VALC                 = 6,  // Reads valC
READ_CLOSE_PAREN          = 7,  // Reads ")"
READ_BEGIN                = 8,  // Reads the "begin" keyword
READ_ASSIGNMENT_VAR       = 9,  // Reads the variable for assignment (p_var)
READ_ASSIGNMENT_OPERATOR  = 10, // Reads the assignment operator (<= or =)
READ_CONST1               = 11, // Reads the constant for true branch
READ_SEMICOLON1           = 12, // Expects semicolon ";" after const1
READ_END1                 = 13, // Reads the "end" keyword for true branch
READ_ELSE                 = 14, // Expects the "else" keyword
READ_BEGIN2               = 16, // Reads the "begin" keyword for false branch
READ_ASSIGNMENT_VAR2      = 18, // Reads the variable for false branch
READ_ASSIGNMENT_OPERATOR2 = 19, // Reads the assignment operator for false branch
READ_CONST2               = 20, // Reads the constant for false branch
READ_SEMICOLON2           = 21, // Expects semicolon ";" after const2
READ_END2                 = 22, // Reada the "end" keyword for false branch
EVALUATE                  = 23, // Evaluates stuff 
ERROR                     = 24; // Error state, machine remains here until rst
```

## Code Documentation

... to be done.
