module if_else_parser_tb();

    reg clk, rst;
    reg [31:0] x;
    reg [6:0] ascii_char;
    reg char_valid;
    wire [31:0] p;
    wire parsing_done;
    wire error_flag;

    // Instantiate the parser
    if_else_parser uut (
        .clk(clk),
        .rst(rst),
        .x(x),
        .ascii_char(ascii_char),
        .char_valid(char_valid),
        .p(p),
        .parsing_done(parsing_done),
        .error_flag(error_flag)
    );

    // Generate a clock: 10 ns period
    always #5 clk = ~clk;

    // Task to send a character for one cycle.
    task send_char(input [6:0] ch);
    begin
        ascii_char = ch;
        char_valid = 1;
        #10; // one half clock period
        // Let the rising edge be captured
        #10;
        char_valid = 0;
        #10;
    end
    endtask

    // Test stimulus: sending the code: 
    // "if(x==5)begin p<=20;endelsebegin p<=30;end"
    initial begin
        clk = 0;
        rst = 1;
        x = 6;      
        char_valid = 0;
        #20;
        rst = 0;
        // "if"
        send_char("i");  
        send_char("f");
        // "("
        send_char("(");
        // "x"
        send_char("x");
        // "==" (or any other comparator)
        send_char(">");  
        // send_char("=");  
        // valC
        send_char("5");
        // ")"
        send_char(")");
        // ini the beninging ("begin")
        send_char("b");  
        send_char("e");  
        send_char("g");  
        send_char("i");  
        send_char("n");
        // whitespace (parser ignores it)
        send_char(" ");
        // "p"
        send_char("p");
        // "<=" (nonblocking assignment)
        send_char("<");  
        send_char("=");
        // const1
        send_char("2");
        send_char("0");
        // ";"
        send_char(";");
        // "end"
        send_char("e");
        send_char("n");
        send_char("d");
        // "else"
        send_char("e");
        send_char("l");
        send_char("s");
        send_char("e");
        // "begin"
        send_char("b");
        send_char("e");
        send_char("g");
        send_char("i");
        send_char("n");
        // whitespace
        send_char(" ");
        // "p"
        send_char("p");
        // "<=" (nonbloking assignment)
        send_char("<");
        send_char("=");
        // const2
        send_char("3");
        send_char("0");
        // ";"
        send_char(";");
        // "end"
        send_char("e");
        send_char("n");
        send_char("d");

        // Wait some cycles for the parser to finish processing
        // #100;
        if (parsing_done && !error_flag)
            $display("Test passed. p = %d", p);
        else if (error_flag)
            $display("Error detected in parser.");
        else
            $display("Parsing not finished.");
        $finish;
    end

endmodule
