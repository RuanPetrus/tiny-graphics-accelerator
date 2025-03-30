`default_nettype none

`define S_IDLE 0
`define S_START 1
`define S_ORDER 2
`define S_INIT_EQ 3
`define S_INIT_ZBUFFER 4
`define S_FIND_POINT 5

`define MAX(a, b) ((a) > (b) ? (a) : (b))
`define MIN(a, b) ((a) < (b) ? (a) : (b))

`define SHIFT_RIGHT_ROUND(a, b) (((a) + ((1 << (b)) -1)) >>> (b))
`define INV_NEAR_PLANE 32'sd327680
`define INV_FAR_PLANE 32'sd4096

module Rasterizer(
				i_clk, i_start, 
				i_v1, i_v2, i_v3, // Values must be 28.4 fixed point
				i_iz1, i_iz2, i_iz3, // This should be 1/z in 16.16 fixed point

				// Assuming Zbuffer memory 1 cycle write, 1 cycle read
				i_zbuffer_data,
				o_zbuffer_addr, o_zbuffer_data,

				o_write, o_done, o_point
				);
   parameter SCREEN_WIDTH = 320;
   parameter SCREEN_HEIGHT = 240;

   input     i_clk, i_start;
   input [63:0]	i_v1, i_v2, i_v3; // {y, x} fixed_point, 28:4 bits each
   input [31:0]	i_iz1, i_iz2, i_iz3; // This should be 1/z in 16.16 fixed point

   input [31:0]	i_zbuffer_data;
   output wire [16:0] o_zbuffer_addr;
   output reg [31:0] o_zbuffer_data;

   output reg		 o_write, o_done;
   output reg [31:0] o_point; // {y, x} int 16 bits each

   reg signed [31:0] x1, x2, x3;
   reg signed [31:0] y1, y2, y3;
   reg signed [31:0] next_x1, next_x2, next_x3;
   reg signed [31:0] next_y1, next_y2, next_y3;
   wire		     order_expr;

   // output reg [3:0]	     state;
   reg [3:0]	     state;
   reg [3:0]	     next_state;

   wire		     point_inside;
   reg		     next_o_write;
   reg		     next_o_done;
   reg [31:0]	     next_o_point;

   wire signed [31:0] minxf, minyf, maxxf, maxyf;
   wire signed [31:0] minx, miny, maxx, maxy;
   wire signed [31:0] dx21, dx32, dx13, dx31, dy21, dy32, dy13, dy31;

   wire signed [31:0] fdx21, fdx32, fdx13, fdy21, fdy32, fdy13;
   wire signed [31:0] eq1init, eq2init, eq3init;
   wire signed [31:0] eq1xinc, eq2xinc, eq3xinc;
   wire signed [31:0] eq1yinc, eq2yinc, eq3yinc;

   reg signed [31:0]  eq1y, eq2y, eq3y, eq1x, eq2x, eq3x;
   reg signed [31:0]  next_eq1y, next_eq2y, next_eq3y, next_eq1x, next_eq2x, next_eq3x;

   reg signed [31:0]  x, y;
   reg signed [31:0]  next_x, next_y;

   reg signed [31:0]  ahead_zbuffer_data;

   reg signed [31:0]  next_o_zbuffer_data;

   wire [16:0]		  zbuffer_init, zbuffer_max;
   reg [16:0]		  zbuffer_x, zbuffer_y;
   reg [16:0]		  next_zbuffer_x, next_zbuffer_y;

   assign o_zbuffer_addr = zbuffer_x;
   // Zbuffer related

   always @(posedge i_clk) begin
      state <= next_state;
      o_done <= next_o_done;
      o_write <= next_o_write;

      x1 <= next_x1;
      x2 <= next_x2;
      x3 <= next_x3;
      y1 <= next_y1;
      y2 <= next_y2;
      y3 <= next_y3;

      iz1 <= next_iz1;
      iz2 <= next_iz2;
      iz3 <= next_iz3;

      x <= next_x;
      y <= next_y;

      eq1y <= next_eq1y;
      eq2y <= next_eq2y;
      eq3y <= next_eq3y;

      eq1x <= next_eq1x;
      eq2x <= next_eq2x;
      eq3x <= next_eq3x;

      eqzx <= next_eqzx;
      eqzy <= next_eqzy;

      o_point <= next_o_point;

      ahead_zbuffer_data <= i_zbuffer_data;

	  o_zbuffer_data <= next_o_zbuffer_data;

	  zbuffer_x <= next_zbuffer_x;
	  zbuffer_y <= next_zbuffer_y;
      
      if (i_start && state == `S_IDLE) begin
		 state <= `S_START;
		 o_done <= 0;
		 o_write <= 0;
		 x1 <= i_v1[31:0];
		 x2 <= i_v2[31:0];
		 x3 <= i_v3[31:0];
		 y1 <= i_v1[63:32];
		 y2 <= i_v2[63:32];
		 y3 <= i_v3[63:32];
		 
		 iz1 <= i_iz1;
		 iz2 <= i_iz2;
		 iz3 <= i_iz3;
      end
   end

   // State related logic
   initial state = `S_IDLE;
   always @(*)  begin
      if      (state == `S_START)   next_state = `S_ORDER;
      else if (state == `S_ORDER)   next_state = `S_INIT_EQ;
      else if (state == `S_INIT_EQ) begin
		 if ((iz1 <= 0 || iz1 > `INV_NEAR_PLANE) && 
	 		 (iz2 <= 0 || iz2 > `INV_NEAR_PLANE) && 
	 		 (iz3 <= 0 || iz3 > `INV_NEAR_PLANE)) next_state = `S_IDLE;
		 
		 else next_state = `S_INIT_ZBUFFER;
	  end

      else if (state == `S_INIT_ZBUFFER) next_state = `S_FIND_POINT;
      else if (state == `S_FIND_POINT) begin
		 if (x +1 >= maxx && y + 1 >= maxy) next_state = `S_IDLE;
		 else                               next_state = `S_FIND_POINT;
      end
      else next_state = state;
   end

   // Write point logic
   initial o_done = 1;
   initial o_write = 0;
   initial zbuffer_x = 0;

   // assign point_inside = (eq1x > 0 && eq2x > 0 && eq3x > 0 && curr_zbuffer_data < eqzx && eqzx < `INV_NEAR_PLANE);
   assign point_inside = (eq1x > 0 && eq2x > 0 && eq3x > 0 
						  && ahead_zbuffer_data < eqzx && eqzx < `INV_NEAR_PLANE
						  && 0 <= x && x < SCREEN_WIDTH && 0 <= y && y < SCREEN_HEIGHT);

   // assign point_inside = (eq1x > 0 && eq2x > 0 && eq3x > 0 && curr_zbuffer_data >= 0);

   always @(*) begin
      // Done
      if (state == `S_IDLE 
		  || (state == `S_FIND_POINT && x +1 >= maxx && y + 1 >= maxy)
		  || (state == `S_INIT_EQ && ((iz1 <= 0 || iz1 > `INV_NEAR_PLANE) && 
	 								  (iz2 <= 0 || iz2 > `INV_NEAR_PLANE) && 
	 								  (iz3 <= 0 || iz3 > `INV_NEAR_PLANE)))) 
		next_o_done = 1;
      else next_o_done = 0;

      // Point write
      if (state == `S_FIND_POINT && point_inside) next_o_write = 1;
      else next_o_write = 0;

      // Point
      if (state == `S_FIND_POINT && point_inside) next_o_point = {y[15:0], x[15:0]};
      else next_o_point = o_point;

      // Zbuffer value
      if (state == `S_FIND_POINT && point_inside) next_o_zbuffer_data = eqzx;
      else next_o_zbuffer_data = o_zbuffer_data;
   end 

   // x, y related logic
   assign minxf = `MIN(`MIN(x1, x2), x3);
   assign minx = `MAX((minxf + 32'sh0F) >>> 4, 0);

   assign minyf = `MIN(`MIN(y1, y2), y3);
   assign miny = `MAX((minyf + 32'sh0F) >>> 4, 0);

   assign maxxf = `MAX(`MAX(x1, x2), x3);
   assign maxx = `MIN((maxxf + 32'sh0F) >>> 4, SCREEN_WIDTH);
   
   assign maxyf = `MAX(`MAX(y1, y2), y3);
   assign maxy = `MIN((maxyf + 32'sh0F) >>> 4, SCREEN_HEIGHT);

   assign dx21 = x2 - x1;
   assign dx32 = x3 - x2;
   assign dx13 = x1 - x3;
   assign dx31 = x3 - x1;

   assign dy21 = y2 - y1;
   assign dy32 = y3 - y2;
   assign dy13 = y1 - y3;
   assign dy31 = y3 - y1;

   assign fdx21 = dx21 <<< 4;
   assign fdx32 = dx32 <<< 4;
   assign fdx13 = dx13 <<< 4;

   assign fdy21 = dy21 <<< 4;
   assign fdy32 = dy32 <<< 4;
   assign fdy13 = dy13 <<< 4;

   assign eq1xinc = eq1x - fdy21;
   assign eq2xinc = eq2x - fdy32;
   assign eq3xinc = eq3x - fdy13;

   assign eq1yinc = eq1y + fdx21;
   assign eq2yinc = eq2y + fdx32;
   assign eq3yinc = eq3y + fdx13;

   assign eq1init = dx21*((miny <<< 4) - y1) - dy21 * ((minx <<< 4) - x1);
   assign eq2init = dx32*((miny <<< 4) - y2) - dy32 * ((minx <<< 4) - x2);
   assign eq3init = dx13*((miny <<< 4) - y3) - dy13 * ((minx <<< 4) - x3);

   // Zbuffer logic
   reg signed [31:0] iz1, iz2, iz3;
   reg signed [31:0] next_iz1, next_iz2, next_iz3;

   wire signed [31:0] dz13, dz23;
   wire signed [63:0] areat, area;
   wire signed [63:0] fdzxt, fdzyt, eqzyt, fdzyt_temp;
   wire signed [31:0] eqzinit_temp;
   wire signed [31:0] fdzx, fdzy, eqzinit;
   
   assign dz13 = iz1 - iz3;
   assign dz23 = iz2 - iz3;

   assign areat = dx21*dy31 - dy21 * dx31;
   assign area = (areat == 0 ? 1 : areat);

   assign fdzyt_temp = dz13*dx32 + dz23*dx13;
   
   assign fdzxt = -((dz13*dy32 + dz23*dy13) <<< 16)/area;
   assign fdzyt = (fdzyt_temp <<< 16)/area;

   assign fdzx = 32'`SHIFT_RIGHT_ROUND(fdzxt, 12);
   assign fdzy = 32'`SHIFT_RIGHT_ROUND(fdzyt, 12);

   assign eqzyt = ((eq2init*dz13 + eq3init*dz23) <<< 16)/area;
   assign eqzinit_temp = 32'`SHIFT_RIGHT_ROUND(eqzyt, 16);
   assign eqzinit = iz3 + eqzinit_temp;

   reg signed [31:0]  eqzx, eqzy;
   reg signed [31:0]  next_eqzy, next_eqzx;
   wire signed [31:0] eqzyinc, eqzxinc;

   assign eqzyinc = eqzy + fdzy;
   assign eqzxinc = eqzx + fdzx;

   always @(*) begin
      if (state == `S_INIT_EQ) begin 
		 next_x = minx;
		 next_y = miny;

		 next_eq1y = eq1init;
		 next_eq2y = eq2init;
		 next_eq3y = eq3init;

		 next_eq1x = eq1init;
		 next_eq2x = eq2init;
		 next_eq3x = eq3init;

		 next_eqzy = eqzinit;
		 next_eqzx = eqzinit;
      end
      else if (state == `S_FIND_POINT && x + 1 >= maxx) begin
		 next_x = minx;
		 next_y = y + 1;

		 next_eq1y = eq1yinc;
		 next_eq2y = eq2yinc;
		 next_eq3y = eq3yinc;

		 next_eq1x = eq1yinc;
		 next_eq2x = eq2yinc;
		 next_eq3x = eq3yinc;

		 next_eqzy = eqzyinc;
		 next_eqzx = eqzyinc;
      end
      else if (state == `S_FIND_POINT) begin
		 next_x = x + 1;
		 next_y = y;

		 next_eq1y = eq1y;
		 next_eq2y = eq2y;
		 next_eq3y = eq3y;

		 next_eq1x = eq1xinc;
		 next_eq2x = eq2xinc;
		 next_eq3x = eq3xinc;

		 next_eqzy = eqzy;
		 next_eqzx = eqzxinc;
      end
      else begin
		 next_x = x;
		 next_y = y;

		 next_eq1y = eq1y;
		 next_eq2y = eq2y;
		 next_eq3y = eq3y;

		 next_eq1x = eq1x;
		 next_eq2x = eq2x;
		 next_eq3x = eq3x;

		 next_eqzy = eqzy;
		 next_eqzx = eqzx;
      end
   end

   // xs and ys related logic ordering
   assign order_expr = (y2 - y1) * (x3 - x2) - (x2 - x1) * (y3 - y2) > 0;

   always @(*) begin
      next_x1 = x1;
      next_y1 = y1;

      next_iz1 = iz1;
      if (state == `S_ORDER && order_expr) begin
		 next_x2 = x3;
		 next_x3 = x2;
		 next_y2 = y3;
		 next_y3 = y2;

		 next_iz2 = iz3;
		 next_iz3 = iz2;
      end
      else begin
		 next_x2 = x2;
		 next_x3 = x3;
		 next_y2 = y2;
		 next_y3 = y3;

		 next_iz2 = iz2;
		 next_iz3 = iz3;
      end
   end

   // Zbuffer related control
   assign zbuffer_max = 17'(zbuffer_y + 17'(maxx - minx));
   assign zbuffer_init = 17'((miny * SCREEN_WIDTH) + minx);

   always @(*) begin
      if (state == `S_INIT_EQ) begin 
		 next_zbuffer_x = zbuffer_init;
		 next_zbuffer_y = zbuffer_init;
      end
      else if (state == `S_INIT_ZBUFFER) begin
		 if (zbuffer_x + 1 >= zbuffer_max) begin
			next_zbuffer_x = zbuffer_y + SCREEN_WIDTH;
			next_zbuffer_y = zbuffer_y + SCREEN_WIDTH;
		 end
		 else begin
			next_zbuffer_x = zbuffer_x + 1;
			next_zbuffer_y = zbuffer_y;
		 end
	  end
      else if (state == `S_FIND_POINT && zbuffer_x + 1 >= zbuffer_max) begin
		 next_zbuffer_x = zbuffer_y + SCREEN_WIDTH;
		 next_zbuffer_y = zbuffer_y + SCREEN_WIDTH;
      end
      else if (state == `S_FIND_POINT) begin
		 next_zbuffer_x = zbuffer_x + 1;
		 next_zbuffer_y = zbuffer_y;
      end
      else begin
		 next_zbuffer_x = zbuffer_x;
		 next_zbuffer_y = zbuffer_y;
      end
   end
   

endmodule

