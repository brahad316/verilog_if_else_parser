#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void generate_testbench(const char *input_filename, const char *output_filename, int x) {
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
    fprintf(output_file, "    reg [31:0] x;\n");
    fprintf(output_file, "    reg [6:0] ascii_char;\n");
    fprintf(output_file, "    reg char_valid;\n");
    fprintf(output_file, "    wire [31:0] p;\n");
    fprintf(output_file, "    wire parsing_done;\n");
    fprintf(output_file, "    wire error_flag;\n\n");
    fprintf(output_file, "    // Instantiate the parser\n");
    fprintf(output_file, "    if_else_parser uut (\n");
    fprintf(output_file, "        .clk(clk),\n");
    fprintf(output_file, "        .rst(rst),\n");
    fprintf(output_file, "        .x(x),\n");
    fprintf(output_file, "        .ascii_char(ascii_char),\n");
    fprintf(output_file, "        .char_valid(char_valid),\n");
    fprintf(output_file, "        .p(p),\n");
    fprintf(output_file, "        .parsing_done(parsing_done),\n");
    fprintf(output_file, "        .error_flag(error_flag)\n");
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
    
    // Write the initial test setup
    fprintf(output_file, "    // Test stimulus: sending the code from input.v\n");
    fprintf(output_file, "    initial begin\n");
    fprintf(output_file, "        clk = 0;\n");
    fprintf(output_file, "        rst = 1;\n");

    fprintf(output_file, "        x = %d;\n", x);
    
    fprintf(output_file, "        char_valid = 0;\n");
    fprintf(output_file, "        #20;\n");
    fprintf(output_file, "        rst = 0;\n\n");

    // Process each character from the input file
    for (int i = 0; i < file_size; i++) {
        char current_char = input_buffer[i];
        char prev_char = i > 0 ? input_buffer[i - 1] : '\0';
        
        // Skip whitespace characters
        if (current_char == ' ' || current_char == '\t' || current_char == '\n' || current_char == '\r') {
            continue;
        }

        // Check if the character should be skipped
        if (!isalnum(current_char) && 
        current_char != '<' && current_char != '>' && 
        current_char != '=' && current_char != ';' && 
        current_char != '(' && current_char != ')') {
        continue;
        }
        
        // Generate send_char statement for this character
        // fprintf(output_file, "        send_char(\"%c\");\n", current_char);

        if(prev_char == '-' && isdigit(current_char)) {
            fprintf(output_file, "        send_char(\"-\");\n");
            fprintf(output_file, "        send_char(\"%c\");\n", current_char);
        }
        else{
            fprintf(output_file, "        send_char(\"%c\");\n", current_char);
        }
    }

    // Write the closing part of the testbench
    fprintf(output_file, "\n        // Wait some cycles for the parser to finish processing\n");
    fprintf(output_file, "        if (parsing_done && !error_flag)\n");
    fprintf(output_file, "            $display(\"Test passed. p = %%d\", p);\n");
    fprintf(output_file, "        else if (error_flag)\n");
    fprintf(output_file, "            $display(\"Unexpected error.\");\n");
    fprintf(output_file, "        else if(!error_flag && !parsing_done)\n");
    fprintf(output_file, "            $display(\"Parsing failed, you might be assigning non-integer values, or having different identifiers assigned in the input snippet.\");\n");
    fprintf(output_file, "        $finish;\n");
    fprintf(output_file, "    end\n\n");
    fprintf(output_file, "endmodule\n");

    // Clean up
    free(input_buffer);
    fclose(input_file);
    fclose(output_file);
}

int main() {
    int x;
    printf("Enter the user input x: ");
    scanf("%d", &x);
    generate_testbench("input.v", "if_else_parser_tb_gen.v", x);
    return 0;
}
