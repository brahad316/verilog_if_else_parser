#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void generate_testbench(const char *input_filename, const char *output_filename, int x_value) {
    // Open input file
    FILE *input_file = fopen(input_filename, "r");
    if (!input_file) {
        perror("Failed to open input file");
        return;
    }

    // Open output file
    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("Failed to open output file");
        fclose(input_file);
        return;
    }

    // Write testbench header
    fprintf(output_file, "module if_else_parser_tb();\n\n");
    fprintf(output_file, "    reg clk, rst;\n");
    fprintf(output_file, "    reg signed [31:0] x;\n");
    fprintf(output_file, "    reg [6:0] ascii_char;\n");
    fprintf(output_file, "    reg char_valid;\n");
    fprintf(output_file, "    wire signed [31:0] p;\n");
    fprintf(output_file, "    wire [16*7-1:0] assignment_var;\n");
    fprintf(output_file, "    wire [3:0] assignment_var_length;\n");
    fprintf(output_file, "    wire parsing_done;\n");
    fprintf(output_file, "    wire error_flag;\n");
    fprintf(output_file, "    wire [3:0] error_code;\n\n");
    
    fprintf(output_file, "    // Error codes\n");
    fprintf(output_file, "    parameter NO_ERROR          = 4'd0,\n");
    fprintf(output_file, "              INVALID_KEYWORD   = 4'd1,\n");
    fprintf(output_file, "              VAR_MISMATCH      = 4'd2,\n");
    fprintf(output_file, "              INVALID_CHAR      = 4'd3,\n");
    fprintf(output_file, "              MISSING_SEMICOLON = 4'd4,\n");
    fprintf(output_file, "              MISSING_OPERATOR  = 4'd5,\n");
    fprintf(output_file, "              SYNTAX_ERROR      = 4'd6,\n");
    fprintf(output_file, "              PAREN_MISMATCH    = 4'd7;\n\n");
    
    fprintf(output_file, "    // Instantiate the parser\n");
    fprintf(output_file, "    if_else_parser_2 uut (\n");
    fprintf(output_file, "        .clk(clk),\n");
    fprintf(output_file, "        .rst(rst),\n");
    fprintf(output_file, "        .x(x),\n");
    fprintf(output_file, "        .ascii_char(ascii_char),\n");
    fprintf(output_file, "        .char_valid(char_valid),\n");
    fprintf(output_file, "        .p(p),\n");
    fprintf(output_file, "        .assignment_var(assignment_var),\n");
    fprintf(output_file, "        .assignment_var_length(assignment_var_length),\n");
    fprintf(output_file, "        .parsing_done(parsing_done),\n");
    fprintf(output_file, "        .error_flag(error_flag),\n");
    fprintf(output_file, "        .error_code(error_code)\n");
    fprintf(output_file, "    );\n\n");
    fprintf(output_file, "    // Generate a clock: 10 ns period\n");
    fprintf(output_file, "    always #5 clk = ~clk;\n\n");
    fprintf(output_file, "    // Task to send a character for one cycle.\n");
    fprintf(output_file, "    task send_char(input [6:0] ch);\n");
    fprintf(output_file, "    begin\n");
    fprintf(output_file, "        ascii_char = ch;\n");
    fprintf(output_file, "        char_valid = 1;\n");
    fprintf(output_file, "        #5;\n");
    fprintf(output_file, "        #5; // full clock cycle complete\n");
    fprintf(output_file, "        char_valid = 0;\n");
    fprintf(output_file, "    end\n");
    fprintf(output_file, "    endtask\n\n");

    // write the extract_var_name function
    fprintf(output_file, "    // Helper function to extract variable name from packed format\n");
    fprintf(output_file, "    function [8*16:1] extract_var_name;\n");
    fprintf(output_file, "        input [16*7-1:0] packed_var;\n");
    fprintf(output_file, "        input [3:0] length;\n");
    fprintf(output_file, "        reg [6:0] char;\n");
    fprintf(output_file, "        integer i;\n");
    fprintf(output_file, "    begin\n");
    fprintf(output_file, "        extract_var_name = 0;\n");
    fprintf(output_file, "        for (i = 0; i < length; i = i + 1) begin\n");
    fprintf(output_file, "            char = (packed_var >> (i*7)) & 7'h7F;\n");
    fprintf(output_file, "            extract_var_name[8*(length-i) -: 8] = char;\n");
    fprintf(output_file, "        end\n");
    fprintf(output_file, "    end\n");
    fprintf(output_file, "    endfunction\n\n");

    // Read input file to a buffer
    fseek(input_file, 0, SEEK_END);
    long file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    
    char *input_buffer = (char *)malloc(file_size + 1);
    if (!input_buffer) {
        perror("Memory allocation failed");
        fclose(input_file);
        fclose(output_file);
        return;
    }
    
    fread(input_buffer, 1, file_size, input_file);
    input_buffer[file_size] = '\0';
    
    // Write the initial test setup
    fprintf(output_file, "    // Test stimulus: sending the code from input.v\n");
    fprintf(output_file, "    initial begin\n");
    fprintf(output_file, "        clk = 0;\n");
    fprintf(output_file, "        rst = 1;\n");

    fprintf(output_file, "        // Set the input value that will be used when evaluating the condition\n");
    fprintf(output_file, "        x = %d;\n", x_value);
    
    fprintf(output_file, "        char_valid = 0;\n");
    fprintf(output_file, "        #20;\n");
    fprintf(output_file, "        rst = 0;\n\n");

    fprintf(output_file, "        // Send each character of the if-else code to the parser\n");
    
    // Process each character from the input file
    int i = 0;
    int found_if = 0;
    int found_complete_if_else = 0;
    int block_depth = 0;

    while (i < file_size) {
        char ch = input_buffer[i++];
        
        // Detect beginning of "if" statement
        if (!found_if && i < file_size && ch == 'i' && input_buffer[i] == 'f') {
            found_if = 1;
        }
        
        if (found_if) {
            // Track block depth with braces
            if (ch == '{' || (i >= 5 && strncmp(&input_buffer[i-5], "begin", 5) == 0)) {
                block_depth++;
            }
            else if (ch == '}' || (i >= 3 && strncmp(&input_buffer[i-3], "end", 3) == 0)) {
                block_depth--;
                if (block_depth == 0 && found_complete_if_else) {
                    // End of if-else structure reached - but keep emitting until we're done
                    // i += 3; // Skip "end" so we include it
                }
            }
            
            // Detect else keyword
            if (i >= 4 && strncmp(&input_buffer[i-4], "else", 4) == 0) {
                found_complete_if_else = 1;
            }
            
            // Output character with proper escaping
            if (ch == '\n') {
                fprintf(output_file, "        send_char(\"\\n\");\n");
            } 
            else if (ch == '\t') {
                fprintf(output_file, "        send_char(\"\\t\");\n");
            }
            else if (ch == '\r') {
                // Skip carriage returns
                continue;
            }
            else if (ch == ' ') {
                fprintf(output_file, "        send_char(\" \");\n");
            }
            else if (isprint(ch)) {
                fprintf(output_file, "        send_char(\"%c\");\n", ch);
            }
            else {
                fprintf(output_file, "        send_char(%d);\n", (int)ch);
            }
            
            // If we've completed the if-else structure and also processed the "end", THEN stop
            if (found_complete_if_else && block_depth == 0 && 
                i >= 3 && strncmp(&input_buffer[i-3], "end", 3) == 0) {
                break;
            }
        }
    }

    fprintf(output_file, "\n        // Wait for parsing to complete\n");
    fprintf(output_file, "        wait(parsing_done || error_flag);\n");
    fprintf(output_file, "        #20;\n\n");
    fprintf(output_file, "        // Display results\n");
    fprintf(output_file, "        if (parsing_done && !error_flag) begin\n");
    fprintf(output_file, "            $display(\"Test passed! Value %%d assigned to the variable: %%s. (var length: %%d)\", \n");
    fprintf(output_file, "                    p, extract_var_name(assignment_var, assignment_var_length),\n");
    fprintf(output_file, "                    assignment_var_length);\n");
    fprintf(output_file, "        end\n");
    fprintf(output_file, "        else if (error_flag) begin\n");
    fprintf(output_file, "            $display(\"Error: %%s (%%0d)\", \n");
    fprintf(output_file, "                    error_code == 0 ? \"No Error\" :\n");
    fprintf(output_file, "                    error_code == 1 ? \"Invalid Keyword. One of more of the keywords 'begin', 'end', 'if', 'else' are missing or misspelled.\" :\n");
    fprintf(output_file, "                    error_code == 2 ? \"Variable Mismatch. The variable names in the true and false branch assignments do not match.\" :\n");
    fprintf(output_file, "                    error_code == 3 ? \"Invalid Character. You may have entered a character that is not allowed.\" :\n");
    fprintf(output_file, "                    error_code == 4 ? \"Missing Semicolon\" :\n");
    fprintf(output_file, "                    error_code == 5 ? \"Missing Operator\" :\n");
    fprintf(output_file, "                    error_code == 6 ? \"There seems to be a Syntax Error, incorrect use of parantheses, or use of illegal characters.\" :\n");
    fprintf(output_file, "                    error_code == 7 ? \"Parenthesis Mismatch. You may have mismatched parentheses, or parantheses at invalid places.\" : \"Unknown Error\",\n");
    fprintf(output_file, "                    error_code);\n");
    fprintf(output_file, "        end\n");
    fprintf(output_file, "        else begin\n");
    fprintf(output_file, "            $display(\"Parsing not finished.\");\n");
    fprintf(output_file, "        end\n");
    fprintf(output_file, "        $finish;\n");
    fprintf(output_file, "    end\n\n");
    fprintf(output_file, "endmodule\n");

    // Clean up, baby
    free(input_buffer);
    fclose(input_file);
    fclose(output_file);
    
    printf("Testbench generated successfully. The verilog parser will evaluate your input based on the value you've provided (x = %d), and will do the necessary assignment.\n", x_value);
    printf("To compile the verilog parser and testbench, do: iverilog -o parser_gen if_else_parser_2.v if_else_parser_tb_gen.v\n");
    printf("To run the parser, do: vvp .\\parser_gen\n");
}

int main() {
    int x_value;
    printf("Enter the value to use for condition evaluation: ");
    scanf("%d", &x_value);
    generate_testbench("input.v", "if_else_parser_tb_gen.v", x_value);
    return 0;
}
