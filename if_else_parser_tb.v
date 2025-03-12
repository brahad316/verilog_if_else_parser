module if_else_parser_tb();

    reg clk, rst;
    reg [31:0] x;
    reg [6:0] ascii_char;
    reg char_valid;
    wire [31:0] p;
    wire parsing_done;
    wire error_flag;

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

    always #5 clk = ~clk; // 10ns clock period

    task send_char(input [6:0] char);
        begin
            ascii_char = char;
            char_valid = 1;
            #20; // hold time
            char_valid = 0;
            #20; // setup time
        end
    endtask

    initial begin
        clk = 0;
        rst = 1;
        x = 15;
        char_valid = 0;
        #20;
        rst = 0;

        send_char("i");  send_char("f");
        send_char("x");
        send_char("=");  send_char("=");
        send_char("1");  send_char("0");
        send_char("p");
        send_char("<");  send_char("=");
        send_char("2");  send_char("0");
        send_char("e");  send_char("l");  send_char("s"); send_char("e");
        send_char("p");
        send_char("<");  send_char("=");
        send_char("3");  send_char("0");

        #100;

        if (parsing_done) $display("Test passed. p = %d", p);
        else $display("Parsing failed.");

        $finish;
    end
endmodule
