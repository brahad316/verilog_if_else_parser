module if_else_parser_tb();

    reg clk, rst;
    reg signed [31:0] x;
    reg [6:0] ascii_char;
    reg char_valid;
    wire signed [31:0] p;
    wire [6:0] assignment_var;
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

    // Test stimulus: sending the code from input.v
    initial begin
        clk = 0;
        rst = 1;
        // Set the input value that will be used when evaluating the condition
        x = 2;
        char_valid = 0;
        #20;
        rst = 0;

        // Send each character of the if-else code to the parser
        send_char("i");
        send_char("f");

        send_char(" ");


        send_char("(");

        send_char(" ");

        send_char("(");

        send_char(" ");

        send_char("x");

        send_char(" ");

        send_char(")");

        send_char(" ");

        send_char("<");
        send_char("=");

        send_char(" ");


        send_char("(");

        send_char(" ");

        send_char("5");

        send_char(" ");

        send_char(")");

        send_char(" ");

        send_char(")");

        send_char(" ");


        send_char("b");
        send_char("e");
        send_char("g");
        send_char("i");
        send_char("n");

        send_char(" ");
        // send_char("(");

        send_char("a");

        // send_char(")");
        send_char(" ");


        send_char("<");
        send_char("=");

        send_char(" ");
        // send_char("(");

        send_char("7");
        send_char("3");

        // send_char(")");
        send_char(" ");


        send_char(";");


        send_char(" ");


        send_char("e");
        send_char("n");
        send_char("d");
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

        send_char(" ");


        send_char("a");

        send_char(" ");


        send_char("<");
        send_char("=");

        send_char(" ");


        send_char("3");
        send_char("7");

        send_char(" ");


        send_char(";");

        send_char(" ");


        send_char("e");
        send_char("n");
        send_char("d");

        // Wait some cycles for the parser to finish processing
        #100;
        if (parsing_done && !error_flag)
            $display("Test passed. Assigned value %d to the variable \"%c\"", p, assignment_var);
        else if (error_flag) begin
            case(error_code)
                INVALID_KEYWORD:   $display("Error: Invalid keyword (%d)", error_code);
                VAR_MISMATCH:      $display("Error: Variable mismatch between if/else branches (%d)", error_code);
                INVALID_CHAR:      $display("Error: Invalid character (%d)", error_code);
                MISSING_SEMICOLON: $display("Error: Missing semicolon (%d)", error_code);
                MISSING_OPERATOR:  $display("Error: Missing operator (%d)", error_code);
                SYNTAX_ERROR:      $display("Error: Syntax error (%d)", error_code);
                default:           $display("Error: Unknown error code (%d)", error_code);
            endcase
        end
        else if(!error_flag && !parsing_done)
            $display("Parsing failed, not completed or timed out.");
        $finish;
    end

endmodule
