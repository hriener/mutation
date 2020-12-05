module AlarmSystem (mutation,action,sound,flash,alarm);

	input [2:0] action;
	input mutation;
	output sound,flash,alarm;

/*	OpenAndUnlocked = 0
	OpenAndLocked = 1
	ClosedAndUnlocked = 2 
	ClosedAndLocked = 3
	Armed_Quiet = 4
	Armed_Alarm_FlashAndSound = 5 
	Armed_Alarm_Flash = 6
*/
	reg [3:0] state;
	
	reg sound;
	reg flash;
	reg alarm;
	
	initial begin
		state = 0;
		sound = 0;
		flash = 0;
		alarm = 0;
	end
	
	always @(action) 
	begin
		case (action) 
		//  close 
		0 : 
		begin
			if (state == 0) begin
				state = 2;
			end else if (state == 1) begin
				state = 3;
			end	
		end
		//  open
		1 : 
		begin
			if (state == 2) begin
				state = 0;
			end else if (state == 3) begin
				state = 1;
			end else if (state == 4) begin
				state = 5;
				sound = 1;
				flash = 1;
				alarm = 1;
			end 
		end
		//  lock
		2 : 
		begin
			if (state == 0) begin
				state = 1;
			end else if (state == 2) begin
				state = 3;
			end
		end	
		//  unlock
		3 : 
		begin
			if (state == 1) begin
				state = 0;
			end else if (state == 3) begin
				state = 2;	
			end else if (state == 5) begin
				state = 0;
				sound = 0;
				flash = 0; 
			end else if (state == 6) begin
				state = 0;
				flash = 0;
			end else if (state == 4) begin
				if (!alarm) begin
					state = 2;
				end else begin
					state = 0;
				end
				alarm = 0; 
			end
		end
		// wait 20
		4 :
		begin
			if (state == 3) begin
				state = 4;
			end
		end
		// wait 30
		5 :
		begin
			if (state == 5) begin
				state = 6;
				sound = 0;
			end
		end
		// wait 270
		6 :
		begin
			if (state == 6) begin
				state = 4;
				flash = 0;
			end
		end
		default : 
		begin
			state = state;
		end	
		endcase
	end
endmodule
