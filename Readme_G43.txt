Preparation:
Environment: Linux (apollo)
File: 
PLS_G43.c 
record.txt 
orderBATCH01.dat

Compile the program:
gcc PLS_G43.c -o PLS_G43
There are no additional libraries in the program, and standard compilation statements are used.

Run the program:
./PLS_G43

Enter start and end dates:
Format:	addPERIOD [start date] [end date]
E.g. 		addPERIOD 2024-06-01 2024-06-30
When entering the program, you must first enter a start and end date.

Add single order:
Format:	addORDER [Order Number] [Due Date] [Quantity] [Product Name]
E.g.		addORDER P0001 2024-06-10 2000 Product_A

Add multiple orders:
Format:	addBATCH [Orders in a batch file]
E.g.		addBATCH test_data_G43.dat


Generate the schedule and report:
Format:	runPLS [Algorithm] | printREPORT > [Report file name]
E.g.		runPLS FCFS | printREPORT > PLS_Report_G43.txt

Exit the program:
Format:	exitPLS
This will completely terminate the program after collecting all child processes and closing all pipes and files.
