## Input file structure
The input file must be a verilog (`.v`) file of the follwoing format:
```
if(x == valC) 
    begin 
      p  == const1;
    end
else 
    begin
    p <= const2;
    end
```
Where `valC`, `const1` and `const2` are of data type integer. Any whitespace characters will be treated the way iverilog does.

The second input will be the user input, the corresponding script will prompt for it.
Note that:
- The identifiers in both the true and false branches of the if-else statement must be same and precisely the character `p`.
- The variable for the conditional evaluation must be precisely the character `x`.
- It supports `<=` as the assignment operator.
- It supports integer values for the constants, thus negative values too.
- It supports all the standard verilog comparators (`==`, `!=`, `<=`, `>=`, `<`, `>`).
- Takes care of all whitespace characters (whitespace, tabs, newline char, etc.)

## Parser in C

`carser.c`: This parser written in C reads from an input verilog file `input.v` and parses the content to store the relevant constant values, 
evaluate the condition and do the respective assignment based on the evaluation result. The script will prompt the user for their value of `x`.

To compile and run this, do:

```
gcc carser.c -o carser && ./carser
```

## Verilog interpreter

The scripts `c_parser.c` and `if_else_parser.v` contribute to this task. `c_parser.c` is a pre-processor which reads the input file `input.v` 
and organizes the relevant contents in a useful manner, it also creates the test bench (will be saved as `if_else_parser_tb_gen.v`) 
for our verilog interpreter circuit described by `if_else_parser.v`. The generated test bench feeds the input character by character (7 bit ASCII)
to the interpreter. It also feeds in the user input value for `x`.

`if_else_parser.v` is a circuit that takes in 7 bit ASCII characters, one at a time and performs the task of evaluating our if-else block followed
by doing the necessary variable assignment. Note that:

- The variable for the conditional evaluation must be precisely the character `x`.
- It supports both `<=` and `==` for the assignment operators.
- It supports integer values for the constants, thus negative values too.
- It supports all the standard verilog comparators (`==`, `!=`, `<=`, `>=`, `<`, `>`).
- Takes care of all whitespace characters (whitespace, tabs, newline char, etc.)

To compile and run this, do:

```
gcc .\c_parser.c -o c_parser && .\c_parser
```
```
iverilog -o parser_gen if_else_parser.v if_else_parser_tb_gen.v && vvp .\parser_gen
```
