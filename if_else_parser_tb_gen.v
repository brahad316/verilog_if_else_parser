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
        #5; // full clock cycle completed
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
            // Changed this line to put characters in correct order
            extract_var_name[8*(length-i) -: 8] = char;
        end
    end
    endfunction

    // Test stimulus with multi-character variables
    initial begin
        clk = 0;
        rst = 1;
        x = 5;  // User input value     
        char_valid = 0;
        #20;
        rst = 0;
        
        // Send if statement - character by character
        send_char("i");  
        send_char("f");
        send_char("(");
        send_char("(");
        
        // Send variable name "counter"
        
        // send_char("(");

        send_char("c");
        send_char("o");
        send_char("u");
        send_char("n");
        send_char("t");
        send_char("e");
        send_char("r");
        
        // send_char(")");

        // Send condition operator
        send_char(">");  
        send_char("=");  
        
        // Send condition value
        send_char("(");
        send_char("5");
        send_char(")");
        
        // Close parenthesis 
        send_char(")");
        send_char(")");
        
        // Send begin keyword
        send_char("b");  
        send_char("e");  
        send_char("g");  
        send_char("i");  
        send_char("n");
        
        // Send variable "result" for assignment
        send_char("r");
        send_char("e");
        send_char("s");
        send_char("u");
        send_char("l");
        send_char("t");
        
        // Send assignment operator
        send_char("<");  
        send_char("=");
        
        // Send value (201)
        // send_char("(");
        send_char("-");
        send_char("2");
        send_char("0");
        send_char("1");
        // send_char(")");
        
        // Semicolon and end 
        send_char(";");
        send_char("e");
        send_char("n");
        send_char("d");
        
        // Send else keyword
        send_char("e");
        send_char("l");
        send_char("s");
        send_char("e");
        
        // Send begin keyword
        send_char("b");
        send_char("e");
        send_char("g");
        send_char("i");
        send_char("n");
        
        // Send variable "result" again
        // send_char("(");
        send_char("r");
        send_char("e");
        send_char("s");
        send_char("u");
        send_char("l");
        send_char("t");
        // send_char(")");
        
        // Send assignment operator
        send_char("<");
        send_char("=");
        
        // Send value (30)
        send_char("-");
        send_char("3");
        send_char("0");
        
        // Semicolon and end
        send_char(";");
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
                    error_code == 1 ? "Invalid Keyword" :
                    error_code == 2 ? "Variable Mismatch" :
                    error_code == 3 ? "Invalid Character" :
                    error_code == 4 ? "Missing Semicolon" :
                    error_code == 5 ? "Missing Operator" :
                    error_code == 6 ? "Syntax Error" : "Unknown Error",
                    error_code);
        end
        else begin
            $display("Parsing not finished.");
        end
        $finish;
    end

endmodule
