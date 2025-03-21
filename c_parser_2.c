#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to remove all whitespace characters
void remove_whitespace(char *str) {
    char *dst = str;
    for (char *src = str; *src; src++) {
        if (!isspace(*src)) {
            *dst++ = *src;
        }
    }
    *dst = '\0';  // Null-terminate the compacted string
}

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
    fprintf(output_file, "    reg signed [31:0] x;\n");  // We keep this as x for test input
    fprintf(output_file, "    reg [6:0] ascii_char;\n");
    fprintf(output_file, "    reg char_valid;\n");
    fprintf(output_file, "    wire signed [31:0] p;\n"); // Output is still p in the interface
    fprintf(output_file, "    wire parsing_done;\n");
    fprintf(output_file, "    wire error_flag;\n");
    fprintf(output_file, "    wire [3:0] error_code;\n\n");
    
    // Add error code parameter definitions
    fprintf(output_file, "    // Error codes\n");
    fprintf(output_file, "    parameter NO_ERROR          = 4'd0,\n");
    fprintf(output_file, "              INVALID_KEYWORD   = 4'd1,\n");
    fprintf(output_file, "              VAR_MISMATCH      = 4'd2,\n");
    fprintf(output_file, "              INVALID_CHAR      = 4'd3,\n");
    fprintf(output_file, "              MISSING_SEMICOLON = 4'd4,\n");
    fprintf(output_file, "              MISSING_OPERATOR  = 4'd5,\n");
    fprintf(output_file, "              SYNTAX_ERROR      = 4'd6;\n\n");
    
    fprintf(output_file, "    // Instantiate the parser\n");
    fprintf(output_file, "    if_else_parser_2 uut (\n");
    fprintf(output_file, "        .clk(clk),\n");
    fprintf(output_file, "        .rst(rst),\n");
    fprintf(output_file, "        .x(x),\n");  // The value is passed to the module's 'x' input
    fprintf(output_file, "        .ascii_char(ascii_char),\n");
    fprintf(output_file, "        .char_valid(char_valid),\n");
    fprintf(output_file, "        .p(p),\n");  // Result is still captured in 'p' output
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

    // Remove whitespace characters from the input buffer
    remove_whitespace(input_buffer);
    
    // Write the initial test setup
    fprintf(output_file, "    // Test stimulus: sending the code from input.v\n");
    fprintf(output_file, "    initial begin\n");
    fprintf(output_file, "        clk = 0;\n");
    fprintf(output_file, "        rst = 1;\n");

    // Set the input value - this will be matched to the variable name inside the parser
    fprintf(output_file, "        // Set the input value that will be used when evaluating the condition\n");
    fprintf(output_file, "        x = %d;\n", x_value);
    
    fprintf(output_file, "        char_valid = 0;\n");
    fprintf(output_file, "        #20;\n");
    fprintf(output_file, "        rst = 0;\n\n");

    fprintf(output_file, "        // Send each character of the if-else code to the parser\n");
    
    // Process each character from the input file
    for (int i = 0; i < file_size; i++) {
        char current_char = input_buffer[i];
        
        // Skip whitespace characters
        if (current_char == ' ' || current_char == '\t' || current_char == '\n' || current_char == '\r') {
            continue;
        }

        // Check if the character should be skipped
        if (!isalnum(current_char) && 
        current_char != '<' && current_char != '>' && 
        current_char != '=' && current_char != ';' && 
        current_char != '(' && current_char != ')' &&
        current_char != '!' && current_char != '-') {
            continue;
        }
        
        // Generate send_char statement for this character
        fprintf(output_file, "        send_char(\"%c\");\n", current_char);
    }

    // Write the closing part of the testbench
    fprintf(output_file, "\n        // Wait some cycles for the parser to finish processing\n");
    fprintf(output_file, "        #100;\n");
    fprintf(output_file, "        if (parsing_done && !error_flag)\n");
    fprintf(output_file, "            $display(\"Test passed. Result = %%d\", p);\n");
    fprintf(output_file, "        else if (error_flag) begin\n");
    fprintf(output_file, "            case(error_code)\n");
    fprintf(output_file, "                INVALID_KEYWORD:   $display(\"Error: Invalid keyword (%%d)\", error_code);\n");
    fprintf(output_file, "                VAR_MISMATCH:      $display(\"Error: Variable mismatch between if/else branches (%%d)\", error_code);\n");
    fprintf(output_file, "                INVALID_CHAR:      $display(\"Error: Invalid character (%%d)\", error_code);\n");
    fprintf(output_file, "                MISSING_SEMICOLON: $display(\"Error: Missing semicolon (%%d)\", error_code);\n");
    fprintf(output_file, "                MISSING_OPERATOR:  $display(\"Error: Missing operator (%%d)\", error_code);\n");
    fprintf(output_file, "                SYNTAX_ERROR:      $display(\"Error: Syntax error (%%d)\", error_code);\n");
    fprintf(output_file, "                default:           $display(\"Error: Unknown error code (%%d)\", error_code);\n");
    fprintf(output_file, "            endcase\n");
    fprintf(output_file, "        end\n");
    fprintf(output_file, "        else if(!error_flag && !parsing_done)\n");
    fprintf(output_file, "            $display(\"Parsing failed, not completed or timed out.\");\n");
    fprintf(output_file, "        $finish;\n");
    fprintf(output_file, "    end\n\n");
    fprintf(output_file, "endmodule\n");

    // Clean up
    free(input_buffer);
    fclose(input_file);
    fclose(output_file);
    
    printf("Testbench generated successfully. The parser will:\n");
    printf("1. Read variable names dynamically from your input code\n");
    printf("2. Use the value %d for condition evaluation\n", x_value);
    printf("3. Output the result of the if-else statement\n");
}

int main() {
    int x_value;
    printf("Enter the value to use for condition evaluation: ");
    scanf("%d", &x_value);
    generate_testbench("input.v", "if_else_parser_tb_gen.v", x_value);
    return 0;
}