
`ifdef BSV_ASSIGNMENT_DELAY
`else
  `define BSV_ASSIGNMENT_DELAY
`endif

`ifdef BSV_POSITIVE_RESET
  `define BSV_RESET_VALUE 1'b1
  `define BSV_RESET_EDGE posedge
`else
  `define BSV_RESET_VALUE 1'b0
  `define BSV_RESET_EDGE negedge
`endif

`ifdef BSV_ASYNC_RESET
 `define BSV_ARESET_EDGE_META or `BSV_RESET_EDGE RST
`else
 `define BSV_ARESET_EDGE_META
`endif

`ifdef BSV_RESET_FIFO_HEAD
 `define BSV_ARESET_EDGE_HEAD `BSV_ARESET_EDGE_META
`else
 `define BSV_ARESET_EDGE_HEAD
`endif

// Depth 2 FIFO
module FIFO2(CLK,
             RST,
             D_IN,
             ENQ,
             FULL_N,
             D_OUT,
             DEQ,
             EMPTY_N,
             CLR);

   parameter width = 1;
   parameter guarded = 1'b1;

   input     CLK ;
   input     RST ;
   input [width - 1 : 0] D_IN;
   input                 ENQ;
   input                 DEQ;
   input                 CLR ;

   output                FULL_N;
   output                EMPTY_N;
   output [width - 1 : 0] D_OUT;

   reg                    full_reg_n;
   reg                    empty_reg_n;
   reg [width - 1 : 0]    data0_reg;
   reg [width - 1 : 0]    data1_reg;


  // Derived signals for formal verification:
  wire fv_data0_valid;
  wire fv_data1_valid;
  (* keep *)
  (* promise *)
  wire [width - 1 : 0] fv_data0_reg;
  (* keep *)
  (* promise *)
  wire [width - 1 : 0] fv_data1_reg;
  (* keep *)
  (* promise *)
  wire [1 : 0] num_fifo_tkns;
  assign num_fifo_tkns = {(!full_reg_n), (full_reg_n) && (empty_reg_n)};
  assign fv_data0_valid = (!full_reg_n) || empty_reg_n;
  assign fv_data1_valid = (!full_reg_n);
  assign fv_data0_reg = data0_reg & {width{fv_data0_valid}};
  assign fv_data1_reg = data1_reg & {width{fv_data1_valid}};
  // End

   assign                 FULL_N = full_reg_n ;
   assign                 EMPTY_N = empty_reg_n ;
   assign                 D_OUT = data0_reg ;


   // Optimize the loading logic since state encoding is not power of 2!
   wire                   d0di = (ENQ && ! empty_reg_n ) || ( ENQ && DEQ && full_reg_n ) ;
   wire                   d0d1 = DEQ && ! full_reg_n ;
   wire                   d0h = ((! DEQ) && (! ENQ )) || (!DEQ && empty_reg_n ) || ( ! ENQ &&full_reg_n) ;
   wire                   d1di = (ENQ && (!DEQ) && empty_reg_n && full_reg_n) || (ENQ && DEQ && (!full_reg_n)) ;

`ifdef BSV_NO_INITIAL_BLOCKS
`else // not BSV_NO_INITIAL_BLOCKS
   initial
     begin
        data0_reg   = {((width + 1)/2) {2'b10}} ;
        data1_reg  = {((width + 1)/2) {2'b10}} ;
        empty_reg_n = 1'b0;
        full_reg_n  = 1'b1;
     end // initial begin
`endif // BSV_NO_INITIAL_BLOCKS

   always@(posedge CLK `BSV_ARESET_EDGE_META)
     begin
        if (RST == `BSV_RESET_VALUE)
          begin
             empty_reg_n <= `BSV_ASSIGNMENT_DELAY 1'b0;
             full_reg_n  <= `BSV_ASSIGNMENT_DELAY 1'b1;
          end // if (RST == `BSV_RESET_VALUE)
        else
          begin
             if (CLR)
               begin
                  empty_reg_n <= `BSV_ASSIGNMENT_DELAY 1'b0;
                  full_reg_n  <= `BSV_ASSIGNMENT_DELAY 1'b1;
               end // if (CLR)
             else if ( ENQ && ! DEQ ) // just enq
               begin
                  empty_reg_n <= `BSV_ASSIGNMENT_DELAY 1'b1;
                  full_reg_n <= `BSV_ASSIGNMENT_DELAY ! empty_reg_n ;
               end
             else if ( DEQ && ! ENQ )
               begin
                  full_reg_n  <= `BSV_ASSIGNMENT_DELAY 1'b1;
                  empty_reg_n <= `BSV_ASSIGNMENT_DELAY ! full_reg_n;
               end // if ( DEQ && ! ENQ )
          end // else: !if(RST == `BSV_RESET_VALUE)

     end // always@ (posedge CLK or `BSV_RESET_EDGE RST)


   always@(posedge CLK `BSV_ARESET_EDGE_HEAD)
     begin
`ifdef BSV_RESET_FIFO_HEAD
        if (RST == `BSV_RESET_VALUE)
          begin
             data0_reg <= `BSV_ASSIGNMENT_DELAY {width {1'b0}} ;
             data1_reg <= `BSV_ASSIGNMENT_DELAY {width {1'b0}} ;
          end
        else
`endif
          begin
             data0_reg  <= `BSV_ASSIGNMENT_DELAY
                           {width{d0di}} & D_IN | {width{d0d1}} & data1_reg | {width{d0h}} & data0_reg ;
             data1_reg <= `BSV_ASSIGNMENT_DELAY
                          d1di ? D_IN : data1_reg ;
          end // else: !if(RST == `BSV_RESET_VALUE)
     end // always@ (posedge CLK or `BSV_RESET_EDGE RST)



   //always@(posedge CLK)
     //begin: error_checks
        //reg deqerror, enqerror ;
//
        //deqerror =  0;
        //enqerror = 0;
        //if (RST == ! `BSV_RESET_VALUE)
          //begin
             //if ( ! empty_reg_n && DEQ )
               //begin
                  //deqerror =  1;
                  //$display( "Warning: FIFO2: %m -- Dequeuing from empty fifo" ) ;
               //end
             //if ( ! full_reg_n && ENQ && (!DEQ || guarded) )
               //begin
                  //enqerror = 1;
                  //$display( "Warning: FIFO2: %m -- Enqueuing to a full fifo" ) ;
               //end
          //end
     //end // always@ (posedge CLK)

endmodule
