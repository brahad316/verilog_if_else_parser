module if_else_parser_tb();

    reg clk, rst;
    reg signed [31:0] x;
    reg [6:0] ascii_char;
    reg char_valid;
    wire signed [31:0] p;
    wire [16*7-1:0] assignment_var;
    wire [3:0] assignment_var_length;
    wire parsing_done;
    wire error_flag;
    wire [3:0] error_code;

    // Error codes
    parameter NO_ERROR          = 4'd0,
              INVALID_KEYWORD   = 4'd1,
              VAR_MISMATCH      = 4'd2,
              INVALID_CHAR      = 4'd3,
              MISSING_SEMICOLON = 4'd4,
              MISSING_OPERATOR  = 4'd5,
              SYNTAX_ERROR      = 4'd6;

    // Instantiate the parser
    if_else_parser_2 uut (
        .clk(clk),
        .rst(rst),
        .x(x),
        .ascii_char(ascii_char),
        .char_valid(char_valid),
        .p(p),
        .assignment_var(assignment_var),
        .assignment_var_length(assignment_var_length),
        .parsing_done(parsing_done),
        .error_flag(error_flag),
        .error_code(error_code)
    );

    // Generate a clock: 10 ns period
    always #5 clk = ~clk;

    // Task to send a character for one cycle.
    task send_char(input [6:0] ch);
    begin
        ascii_char = ch;
        char_valid = 1;
        #5;
        #5; // full clock cycle complete
        char_valid = 0;
    end
    endtask

    // Helper function to extract variable name from packed format
    function [8*16:1] extract_var_name;
        input [16*7-1:0] packed_var;
        input [3:0] length;
        reg [6:0] char;
        integer i;
    begin
        extract_var_name = 0;
        for (i = 0; i < length; i = i + 1) begin
            char = (packed_var >> (i*7)) & 7'h7F;
            extract_var_name[8*(length-i) -: 8] = char;
        end
    end
    endfunction

    // Test stimulus: sending the code from input.v
    initial begin
        clk = 0;
        rst = 1;
        // Set the input value that will be used when evaluating the condition
        x = 43;
        char_valid = 0;
        #20;
        rst = 0;

        // Send each character of the if-else code to the parser
        send_char("i");
        send_char("f");
        send_char(" ");
        send_char("(");
        send_char("(");
        send_char("x");
        send_char("_");
        send_char("v");
        send_char("a");
        send_char("r");
        send_char("1");
        send_char(")");
        send_char(" ");
        send_char("=");
        send_char("=");
        send_char(" ");
        send_char(" ");
        send_char("(");
        send_char("4");
        send_char("2");
        send_char(")");
        send_char(")");
        send_char(" ");
        send_char("b");
        send_char("e");
        send_char("g");
        send_char("i");
        send_char("n");
        send_char("\n");
        send_char(" ");
        send_char(" ");
        send_char(" ");
        send_char(" ");
        send_char("p");
        send_char("_");
        send_char("v");
        send_char("a");
        send_char("r");
        send_char("_");
        send_char("1");
        send_char(" ");
        send_char("<");
        send_char("=");
        send_char(" ");
        send_char("1");
        send_char("0");
        send_char("0");
        send_char(";");
        send_char("\n");
        send_char("e");
        send_char("n");
        send_char("d");
        send_char(" ");
        send_char("e");
        send_char("l");
        send_char("s");
        send_char("e");
        send_char(" ");
        send_char("b");
        send_char("e");
        send_char("g");
        send_char("i");
        send_char("n");
        send_char("\n");
        send_char(" ");
        send_char(" ");
        send_char(" ");
        send_char(" ");
        send_char("p");
        send_char("_");
        send_char("v");
        send_char("a");
        send_char("r");
        send_char("_");
        send_char("1");
        send_char(" ");
        send_char("<");
        send_char("=");
        send_char(" ");
        send_char("2");
        send_char("0");
        send_char("0");
        send_char(";");
        send_char("\n");
        send_char("e");
        send_char("n");
        send_char("d");

        // Wait for parsing to complete
        wait(parsing_done || error_flag);
        #20;

        // Display results
        if (parsing_done && !error_flag) begin
            $display("Test passed! Value %d assigned to the variable: %s. (var length: %d)", 
                    p, extract_var_name(assignment_var, assignment_var_length),
                    assignment_var_length);
        end
        else if (error_flag) begin
            $display("Error: %s (%0d)", 
                    error_code == 0 ? "No Error" :
                    error_code == 1 ? "Invalid Keyword. One of more of the keywords 'begin', 'end', 'if', 'else' are missing or misspelled." :
                    error_code == 2 ? "Variable Mismatch. The variable names in the true and false branch assignments do not match." :
                    error_code == 3 ? "Invalid Character. You may have entered a character that is not allowed." :
                    error_code == 4 ? "Missing Semicolon." :
                    error_code == 5 ? "Missing Operator." :
                    error_code == 6 ? "Syntax Error." :
                    error_code == 7 ? "Parenthesis Mismatch. You may have mismatched parentheses, or parantheses at invalid places." : "Unknown Error",
                    error_code);
        end
        else begin
            $display("Parsing not finished.");
        end
        $finish;
    end

endmodule
