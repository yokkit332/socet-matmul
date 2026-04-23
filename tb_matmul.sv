module tb_intro_systems_accelerator();
    // Define parameters
    localparam CLK_PERIOD = 10;

    localparam ADDR_WIDTH   = 32;
    localparam DATA_WIDTH   = 32;
    localparam STROBE_WIDTH =  4;

    // tb signals
    logic CLK, nRST;
    int test_num;
    string test_name;
    logic [DATA_WIDTH-1:0] test_data;
    logic [ADDR_WIDTH-1:0] test_addr;

    // Bus interface
    bus_protocol_if busif();

    // DUT
    intro_systems_accelerator DUT (
        .CLK(CLK),
        .nRST(nRST),
        .busif(busif)
    );

    // Clock gen
    always begin
        CLK = 1'b0;
        #(CLK_PERIOD / 2);
        CLK = 1'b1;
        #(CLK_PERIOD / 2);
    end

    task reset_dut;
    begin
        nRST = 1'b0;
        busif.addr = '0;
        busif.wen = 1'b0;
        busif.ren = 1'b0;
        busif.wdata = '0;
        busif.strobe = '1;

        @(posedge CLK);
        @(posedge CLK);

        @(negedge CLK);
        nRST = 1'b1;

        @(negedge CLK);
        @(negedge CLK);
    end
    endtask

    task write_word;
        input logic [31:0] addr;
        input logic [31:0] data;
    begin
        @(posedge CLK);
        busif.addr = addr;
        busif.wen = 1'b1;
        busif.ren = 1'b0;
        busif.wdata = data;
        @(posedge CLK);
        busif.wen = 1'b0;
    end
    endtask


    task read_word;
        input logic [31:0] addr;
        input logic [31:0] exp_data;
    begin
        @(posedge CLK);
        busif.addr = addr;
        busif.wen = 1'b0;
        busif.ren = 1'b1;
        @(negedge CLK);

        // Check that correct data was read/stored
        if (busif.rdata == exp_data) begin
            $info("Read 0x%x from address 0x%x as expected", busif.rdata, addr);
        end else begin
            $error("Read 0x%x from address 0x%x -- incorrect value, expected: 0x%x", busif.rdata, addr, exp_data);
        end

        @(posedge CLK);
        busif.ren = 1'b0;
    end
    endtask

    task wait_for_done;
        input logic[31:0] addr;
    begin
        @(posedge CLK);
        busif.wen = 1'b0;
        busif.ren = 1'b1;
        busif.addr = addr;
        @(negedge CLK);

        // Check that correct data was read/stored
        while(busif.rdata != 32'b1) begin
          @(posedge CLK);
        end
        busif.ren = 1'b0;
    end

    endtask

    //*****************************************************************************
    // Main testbench process
    //*****************************************************************************
    initial begin
        $dumpfile("waveform.fst");
        $dumpvars;
        // Initialize values
        test_name = "Initialization";
        test_num = -1;
        test_data = '0;

        #(0.1);
        reset_dut();

        // TODO: write some tests here!
	
	// addresses: 
	// 0x00 - input reg
	// 0x04 - status reg
	// 0x08 - output reg
	
	// test case 1: input matrix A all 1's
	write_word(32'h0, 32'h01010101);
    write_word(32'h4, 32'h01010101);
    write_word(32'h8, 32'h01010101);
    write_word(32'hC, 32'h01010101);

    // test case 1: input matrix B all 1's
    write_word(32'h10, 32'h01010101);
    write_word(32'h14, 32'h01010101);
    write_word(32'h18, 32'h01010101);
    write_word(32'h1C, 32'h01010101);
    write_word(32'h40, 32'b1);
    wait_for_done(32'h44);

    // expected output all 4's
    // row 1
    read_word(32'h20, 32'h00040004);
    read_word(32'h24, 32'h00040004);

    // row 2
    read_word(32'h28, 32'h00040004);
    read_word(32'h2C, 32'h00040004);

    // row 3
    read_word(32'h30, 32'h00040004);
    read_word(32'h34, 32'h00040004);

    // row 4    
    read_word(32'h38, 32'h00040004);
    read_word(32'h3C, 32'h00040004);

    reset_dut();
    // test case 2: 
	write_word(32'h0, 32'h01020304);
    write_word(32'h4, 32'h05060708);
    write_word(32'h8, 32'h090A0B0C);
    write_word(32'hC, 32'h0D0E0F10);

    // test case 2: input matrix B all 1's
    write_word(32'h10, 32'h01020304);
    write_word(32'h14, 32'h05060708);
    write_word(32'h18, 32'h090A0B0C);
    write_word(32'h1C, 32'h0D0E0F10);
    write_word(32'h40, 32'b1);
    wait_for_done(32'h44);

    // expected output all 4's
    // row 1
    read_word(32'h20, 32'h005A0064);  // C[0][0]=90, C[0][1]=100
    read_word(32'h24, 32'h006E0078);  // C[0][2]=110, C[0][3]=120

    read_word(32'h28, 32'h00CA00E4);  // C[1][0]=202, C[1][1]=228
    read_word(32'h2C, 32'h00FE0118);  // C[1][2]=254, C[1][3]=280

    read_word(32'h30, 32'h013A0164);  // C[2][0]=314, C[2][1]=356
    read_word(32'h34, 32'h018E01B8);  // C[2][2]=398, C[2][3]=440

    read_word(32'h38, 32'h01AA01E4);  // C[3][0]=426, C[3][1]=484
    read_word(32'h3C, 32'h021E0258);  // C[3][2]=542, C[3][3]=600

        $finish;
    end
endmodule
