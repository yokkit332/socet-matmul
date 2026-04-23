module matmul
(
    input logic CLK, nRST,
    bus_protocol_if.peripheral_vital busif
);

    // define addresses of registers
    localparam addr_A = 32'h00000000; // 16 spots for input A, 8 bits each
    localparam addr_B = 32'h00000010; // 16 spots for input B
    localparam addr_C = 32'h00000020; // 16 spots for input C
    localparam addr_start = 32'h00000040; // 1 spot for start, asserting when A and B are loaded
    localparam addr_done = 32'h00000044; // 1 spot for done reg, telling CPU when calculations are done

    // define registers, for 4x4 matmul
    logic [3:0][3:0] [7:0] regA;
    logic [3:0][3:0] [7:0] regB;
    logic [3:0][3:0] [15:0] regC;

    // result signals to accumulate the calculations, to be fed to regC in WRITE state
    logic [3:0][3:0] [15:0] result;

    // define mac signals
    logic mac_en; // tells the mac when to start computing
    logic mac_clr; // tells the mac when to clear its values
  

    // start and done signals
    logic done; // signals if we are done with calculations
    logic start; // signals if CPU done loading values

    // define inner array counter logic signal k 
    logic [1:0] k, k_n;

    // temporary row signals to easily access rows in regA, regB, regC
    logic [1:0] rowA, rowB, rowC;

    // colC determines which half of the column of C we are currently writing to MMIO
    logic colC_half;

    // counter to write to mmio 8 times 
    logic [3:0] mmio_write_count, mmio_write_count_n;
    // define FSM states
    typedef enum logic [2:0] {
        IDLE,
        LOAD,
        COMPUTE,
        TRANSFER,
        WRITE,
	DONE
    } state_t;
    state_t state, next_state;

    // default values
    assign busif.error = 1'b0;
    assign busif.request_stall = 1'b0;

    // calculate which row to access during IO
    always_comb begin
            // divide by 4 to get rows A and B because they are 8 bits each and each read is 32 bits
            rowA = 2'((busif.addr - addr_A) >> 2);
            rowB = 2'((busif.addr - addr_B) >> 2);

            // divide by 8 to get row c because row C is 16 bits each and each write is 32 bits
            rowC = 2'((busif.addr - addr_C) >> 3);

            // divide offset by 4, then determine if even or odd. 
            // even: col 0&1, odd: col 2&3
            colC_half = 1'(((busif.addr - addr_C) /4) % 2);

    end
    
    // MMIO writing to our registers - input logic
    always_ff @(posedge CLK, negedge nRST) begin
        // check for reset signal
        if (!nRST) begin
            for (int x = 0; x < 4; x++) begin
                for(int y = 0; y < 4; y++) begin
                    regA[x][y] <= '0;
                    regB[x][y] <= '0;
                end
            end
            start <= '0;
        end

        // load values into array A
        else if(busif.wen && busif.addr < addr_B) begin
            regA[rowA][0] <= busif.wdata[31:24];
            regA[rowA][1] <= busif.wdata[23:16];
            regA[rowA][2] <= busif.wdata[15:8];
            regA[rowA][3] <= busif.wdata[7:0];

        end

        // load values into array B
        else if(busif.wen && busif.addr < addr_start && busif.addr >= addr_B) begin
            regB[rowB][0] <= busif.wdata[31:24];
            regB[rowB][1] <= busif.wdata[23:16];
            regB[rowB][2] <= busif.wdata[15:8];
            regB[rowB][3] <= busif.wdata[7:0];
        end
        // get start signal to start compute
        else if(busif.wen && busif.addr == addr_start) begin
            start <= busif.wdata[0];
        end

        // clear start once computing begins
        else if(state == COMPUTE || state == IDLE) begin
                start <= 1'b0;
        end
    end

    // MMIO reading from our registers - output logic
    always_comb begin
        busif.rdata = 32'b0;
        if(busif.ren) begin
            if(busif.addr >= addr_C && busif.addr < addr_start) begin

                // means we are writing first half of current row
                if(!colC_half) begin
                    busif.rdata = {regC[rowC][0], regC[rowC][1]};

                // means we are writing second half of current row
                end else begin
                    busif.rdata = {regC[rowC][2], regC[rowC][3]};
                end
            end
            else if(busif.addr == addr_done) begin
                busif.rdata = {31'b0, done};
            end
        end

    end

    // matmul controller state logic
    always_ff @(posedge CLK, negedge nRST) begin
        if(!nRST) begin
            state <= IDLE;
        end else begin
            state <= next_state;
        end
    end

    // input logic block
    always_comb begin
        next_state = state;
        case(state)
        IDLE: next_state = (busif.wen && busif.addr < addr_start) ? LOAD : IDLE;
        LOAD: next_state = start ? COMPUTE : LOAD;
        COMPUTE: next_state = (k==3) ? TRANSFER : COMPUTE;
        TRANSFER: next_state = WRITE;
        WRITE: next_state = (mmio_write_count == 4'd8) ? DONE : WRITE;
	DONE: next_state = IDLE;
        default: next_state = IDLE;

        endcase
    end

    // output logic block for combinational outputs
    always_comb begin
        // default state handles clear for every other state other than compute
        done = 1'b0;
        mac_en = 1'b0;
        mac_clr = 1'b1;
	mmio_write_count_n = mmio_write_count;
	k_n = k;
        case(state)

            COMPUTE: begin
                mac_en = 1'b1;
                mac_clr = 1'b0;
		k_n = k+1;
            end

            WRITE: begin
		done = 1'b1;
                if(busif.ren && busif.addr >= addr_C && busif.addr < addr_start)
                    mmio_write_count_n = mmio_write_count + 4'b1;
                
                
            end

	    DONE: begin
		    done = 1'b0;
	        end
            default:begin end


        endcase
    end
    // output logic block for registered outputs
    always_ff @(posedge CLK, negedge nRST) begin
        if(!nRST) begin
            for(int x = 0; x < 4; x++) begin
                for(int y = 0; y < 4; y++) begin
                            regC[x][y] <= 0;
                end
            end
	        mmio_write_count <= 4'b0;
            k <= '0;
        end
        else begin
            case(state)
                IDLE: begin

                    for(int x = 0; x < 4; x++) begin
                        for(int y = 0; y < 4; y++) begin
                            regC[x][y] <= 0;
                        end
                    end

                    k <= '0;
                    mmio_write_count <= '0;
                end

                LOAD: begin 
                    // empty state, CPU loading values into regA and regB
                end

                COMPUTE: begin
                    k <= k_n;

                end

                TRANSFER: begin
                    // write result into regC[i][j]
                    for(int x = 0; x < 4; x++) begin
                        for(int y = 0; y < 4; y++) begin
                            regC[x][y] <= result[x][y];
                        end
                    end
                    k <= '0;
                end
		
                WRITE: begin
                    mmio_write_count <= mmio_write_count_n;
                end
                default:begin end

            endcase
        end
    end
    // instantiate mac modules for each row-col of C
    // start with row 0 col 0 -> row 3 col 3

    // row 0
    mac c00 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[0][k]), .currB(regB[k][0]), .result(result[0][0]));
    mac c01 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[0][k]), .currB(regB[k][1]), .result(result[0][1]));
    mac c02 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[0][k]), .currB(regB[k][2]), .result(result[0][2]));
    mac c03 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[0][k]), .currB(regB[k][3]), .result(result[0][3]));
    
    // row 1
    mac c10 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[1][k]), .currB(regB[k][0]), .result(result[1][0]));
    mac c11 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[1][k]), .currB(regB[k][1]), .result(result[1][1]));
    mac c12 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[1][k]), .currB(regB[k][2]), .result(result[1][2]));
    mac c13 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[1][k]), .currB(regB[k][3]), .result(result[1][3]));
    
    //row 2
    mac c20 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[2][k]), .currB(regB[k][0]), .result(result[2][0]));
    mac c21 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[2][k]), .currB(regB[k][1]), .result(result[2][1]));
    mac c22 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[2][k]), .currB(regB[k][2]), .result(result[2][2]));
    mac c23 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[2][k]), .currB(regB[k][3]), .result(result[2][3]));
   
    // row 3
    mac c30 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[3][k]), .currB(regB[k][0]), .result(result[3][0]));
    mac c31 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[3][k]), .currB(regB[k][1]), .result(result[3][1]));
    mac c32 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[3][k]), .currB(regB[k][2]), .result(result[3][2]));
    mac c33 (.CLK(CLK), .nRST(nRST), .en(mac_en), .clear(mac_clr), .currA(regA[3][k]), .currB(regB[k][3]), .result(result[3][3]));
            
    

    


endmodule

module mac (
    input logic CLK,
    input logic nRST,
    input logic en,
    input logic clear,
    input logic [7:0] currA,
    input logic [7:0] currB,
    output logic [15:0] result
);

    // temp signal that stores next running product
    logic [15:0] acc_next;


    // combinationally drive temp variable to do the MAC calculation
    assign acc_next = result + currA * currB;
    always_ff @(posedge CLK, negedge nRST) begin
        if(!nRST || clear) begin
            result <= '0;
        end

        else if(en) begin
            // current running product to store next running product
            result <= acc_next;

        end
    end      

endmodule
