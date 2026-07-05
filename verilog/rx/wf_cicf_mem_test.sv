
// Copyright (c) 2024-2025 John Seamons, ZL4VO/KF6VO

`timescale 1ns / 100ps

module WF_CICF_MEM_TEST();
    
`include "kiwi.gen.vh"

    reg adc_clk;
    localparam ADC_CLK_PERIOD = 1;
    initial adc_clk = 1'b1;
    always # (ADC_CLK_PERIOD/2.0)
        adc_clk = ~adc_clk;

    reg cpu_clk;
    localparam CPU_CLK_PERIOD = 1;
    initial cpu_clk = 1'b1;
    always # (CPU_CLK_PERIOD/2.0)
        cpu_clk = ~cpu_clk;

    reg rst_wf_wr_A, rst_wf_fir_A, rst_wf_fir_C, run_wf_fir_A, wf_decim_zero_A;
    reg set_wf_fir_tap_C;
    reg [31:0] freeze_tos_A = 0;
    
    reg get_wf_samp_i_C_in, get_wf_samp_q_C_in;
    wire [15:0] wf_dout_C;

    reg [15:0] Lwr_i, Lwr_q;
    reg Lwr;

    localparam N_CHUNKS = 4;
    localparam N_SAMPS_HOST = 4;
    //localparam N_SAMPS_HOST = 1023;

//`define WORKS
`ifdef WORKS
    localparam LOOP_CT = 8;
    localparam N_STATES = 3;
    localparam OVHD = 3;
`else
    localparam LOOP_CT = 2046;
    //localparam N_STATES = 6;
    //localparam N_STATES = 11;
    //localparam OVHD = 3;
    
    localparam N_STATES = 6;
    localparam OVHD = 16;
`endif

    integer i, chunk;
    initial begin
        #0.9;   // setup
        #1; rst_wf_wr_A = 1;
        #1; rst_wf_wr_A = 0;

        #2;
        
        // init ipcore_bram_8k_32b with data pattern
        for (i = 0; i < 1023*2*4; i++) begin
            #(1); Lwr_i = {3'b010, i[12:0]}; Lwr_q = {3'b100, (i[12:0] ^ 13'h1fff)};
                  Lwr = 1;
            #(1); Lwr = 0;
        end
        
        #2;
        
        #1; {set_wf_fir_tap_C, get_wf_samp_i_C_in, get_wf_samp_q_C_in, wf_decim_zero_A} = 0;
        #1; rst_wf_fir_A = 1; rst_wf_fir_C = 1;
        #1; rst_wf_fir_A = 0; rst_wf_fir_C = 0;

        for (chunk = 0; chunk < N_CHUNKS; chunk++) begin
            #1; run_wf_fir_A = 1;
            #1; run_wf_fir_A = 0;
            
            #2;

            // CICF_WAIT_USEC
            #(LOOP_CT * (N_STATES + OVHD))
    
            #2;

            // host read
            for (i = 0; i < N_SAMPS_HOST; i++) begin
                #1; get_wf_samp_i_C_in = 1;
                #1; get_wf_samp_i_C_in = 0; get_wf_samp_q_C_in = 1;
                #1; get_wf_samp_q_C_in = 0;
                // check wf_dout_C[15:0]
            end        
    
            #2;
        end
    end
    

`define LOCAL_MEM
`ifdef LOCAL_MEM
    localparam A_MSB = 12;      // 8k
    wire rd_data_i, rd_data_q;
    
	// Lwr_clk side
    wire Lwr_clk = adc_clk;
    wire Lwr_rst = rst_wf_wr_A;

    reg [A_MSB:0] Lwr_addr;
    reg Lwr_full;
    wire	      Lwr_en = Lwr && ~Lwr_full;
    
    always @ (posedge Lwr_clk)
    begin
        if (Lwr_rst) {Lwr_addr, Lwr_full} <= 0;
        else
        if (Lwr_en) {Lwr_full, Lwr_addr} <= Lwr_addr + 1;
    end

    wire [31:0] Lwr_diq = { Lwr_i[15 -:16], Lwr_q[15 -:16] };

	// Lrd_clk side
    wire Lrd_clk = adc_clk;
    wire Lrd_rst = rst_wf_fir_A;
    wire Lrd_i = rd_data_i;
    wire Lrd_q = rd_data_q;

    reg [A_MSB:0]  Lrd_addr;
	wire [A_MSB:0] Lrd_next = Lrd_addr + Lrd_q;
	
    always @ (posedge Lrd_clk)
        if (Lrd_rst)
            Lrd_addr <= 0;
        else begin
            Lrd_addr <= Lrd_next;
		end
	
	wire [31:0] Lrd_diq;
	wire signed [15:0] wf_data_to_cicf = Lrd_i? Lrd_diq[31 -:16] : Lrd_diq[15 -:16];
	
	// done as an 8kx32b (7.5 BRAM) rather than 8kx16bx2 (8 BRAM)
	ipcore_bram_8k_32b bram_8k_inst (
		.clka	(Lwr_clk),			.clkb	(Lrd_clk),
		.wea	(Lwr_en),
		.addra	(Lwr_addr),			.addrb	(Lrd_next),
		.dina	(Lwr_diq),			.doutb	(Lrd_diq)
	);
`endif


    reg get_wf_samp_i_C, get_wf_samp_q_C;
    always @ (posedge adc_clk)
    begin
        get_wf_samp_i_C <= get_wf_samp_i_C_in;
        get_wf_samp_q_C <= get_wf_samp_q_C_in;
    end

    wf_cicf_mem #(.WIDTH(16)) wf_cicf_mem_inst (
        .adc_clk			(adc_clk),
        
        .wf_data_to_cicf    (wf_data_to_cicf),
        .wf_decim_zero_A    (wf_decim_zero_A),
        // o
        .rd_data_i          (rd_data_i),
        .rd_data_q          (rd_data_q),
        
        .get_wf_samp_i_C    (get_wf_samp_i_C),
        .get_wf_samp_q_C    (get_wf_samp_q_C),
        // o
        .wf_dout_C          (wf_dout_C),

        .cpu_clk			(cpu_clk),
        .freeze_tos_A       (freeze_tos_A),
        
        .set_wf_fir_tap_C   (set_wf_fir_tap_C),
        .rst_wf_fir_A       (rst_wf_fir_A),
        .rst_wf_fir_C       (rst_wf_fir_C),
        .run_wf_fir_A       (run_wf_fir_A)
    );

endmodule
