module if_else_parser (
    input wire clk,                   
    input wire rst,                   
    input wire [31:0] x,              
    input wire [6:0] ascii_char,      
    input wire char_valid,            
    output reg [31:0] p,              
    output reg parsing_done,          
    output reg error_flag             
);

    parameter IDLE = 0, READ_IF = 1, READ_VAR = 2, READ_OPERATOR = 3, READ_VALC = 4,
              READ_ASSIGNMENT_1 = 5, READ_CONST1 = 6, READ_ELSE = 7, 
              READ_ASSIGNMENT_2 = 8, READ_CONST2 = 9, EVALUATE = 10, OUTPUT = 11;

    reg [3:0] state;
    reg [31:0] valC, const1, const2;
    reg [31:0] num_buffer;
    reg parsing_number;
    reg p_detected_if, p_detected_else;
    reg [2:0] comparator;
    reg operator_detected;

    wire is_digit = (ascii_char >= "0" && ascii_char <= "9");

    parameter EQ = 3'b000, NE = 3'b001, LT = 3'b010, GT = 3'b011, LE = 3'b100, GE = 3'b101;

    always @(posedge clk or posedge rst) begin
        if (rst) begin
            state <= IDLE;
            valC <= 0;
            const1 <= 0;
            const2 <= 0;
            num_buffer <= 0;
            parsing_number <= 0;
            parsing_done <= 0;
            error_flag <= 0;
            p_detected_if <= 0;
            p_detected_else <= 0;
            operator_detected <= 0;
        end 
        else begin
            $display("State: %d, char: %c (%h), x: %d, valC: %d, p: %d, done: %b, error: %b", 
             state, ascii_char, ascii_char, x, valC, p, parsing_done, error_flag);
            case (state)
                IDLE: begin
                    if (char_valid && ascii_char == "i") begin
                        state <= READ_IF;
                    end
                end

                READ_IF: begin
                    if (char_valid && ascii_char == "f") begin
                        state <= READ_VAR;
                    end
                end

                READ_VAR: begin
                    if (char_valid && ascii_char == "x") begin
                        state <= READ_OPERATOR;
                    end
                end

                READ_OPERATOR: begin
                    if (char_valid) begin
                        if (ascii_char == "=") begin
                            if (operator_detected) begin
                                comparator <= EQ;
                                state <= READ_VALC;
                            end
                            else begin
                                operator_detected <= 1;
                            end
                        end
                    end
                end

                READ_VALC: begin
                    if (char_valid) begin
                        if (is_digit) begin
                            num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                            parsing_number <= 1;
                        end
                        else if (parsing_number) begin
                            valC <= num_buffer;
                            num_buffer <= 0;
                            parsing_number <= 0;
                            state <= READ_ASSIGNMENT_1;
                        end
                    end
                end

                READ_ASSIGNMENT_1: begin
                    if (char_valid) begin
                        if (ascii_char == "p") begin
                            p_detected_if <= 1;
                        end
                        if (is_digit) begin
                            num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                            parsing_number <= 1;
                        end 
                        else if (parsing_number) begin
                            const1 <= num_buffer;
                            num_buffer <= 0;
                            parsing_number <= 0;
                            state <= READ_ELSE;
                        end
                    end
                end

                READ_ELSE: begin
                    if (char_valid && ascii_char == "e") begin
                        state <= READ_ASSIGNMENT_2;
                    end
                end

                READ_ASSIGNMENT_2: begin
                    if (char_valid) begin
                        if (ascii_char == "p") begin
                            p_detected_else <= 1;
                        end
                        if (is_digit) begin
                            num_buffer <= (num_buffer * 10) + (ascii_char - "0");
                            parsing_number <= 1;
                        end
                        else if (parsing_number) begin
                            const2 <= num_buffer;
                            num_buffer <= 0;
                            parsing_number <= 0;
                            state <= EVALUATE;
                        end
                    end
                end

                EVALUATE: begin
                    if (p_detected_if != p_detected_else) begin
                        error_flag <= 1;
                    end 
                    else begin
                        case (comparator)
                            EQ: p <= (x == valC) ? const1 : const2;
                            default: p <= 0;
                        endcase
                        parsing_done <= 1;
                    end
                    state <= OUTPUT;
                end

                OUTPUT: begin
                    parsing_done <= 1;
                end
            endcase
        end
    end

endmodule
