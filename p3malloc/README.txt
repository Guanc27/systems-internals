# Project: “Strange” Malloc

*NOTE: You should ONLY modify `smalloc.c` file.*

You implementation should NOT depend on any modification in other files.
(You can hack other files for debugging purposes though. 
However, when we grade, we will ONLY take your `smalloc.c` file.)

For project requirements, please check the project document on Canvas.


## How to run tests

To test your memory allocator, you should first run “make” to compile the code. 

After successful compilation, if you want to test your memory allocator on a single trace, you should run: 
./test [-t <tracefile>] [-o <output_filepath>]

For example, you can run:
	./test -t traces/short1-bal.txt -o outputs/short1-bal.out

This will run the memory allocator on trace “traces/short1-bal.txt”, and write the report allocation status to “outputs/short1-bal.out”.

To test your memory allocator on all the 8 given traces, you should run:
	python grading.py

This will output the correctness of your reported status for all the 11 given traces.
(Your report is also saved to `00-grade-malloc.txt`.)

If your memory allocation is implemented correctly, then you should receive 70/70 for the given 11 tests.


## Debugging

You can check the traces in `traces` folder.

We suggest you start from simple tests, such as: fail1.txt, fail2.txt, short1-bal.txt, short2-bal.txt, free-1.txt, free-2.txt, free-3.txt, free-4.txt, and coalesce.txt (ordered).

We suggest you compare your output with the expected outputs in `expected_outputs` folder.
You can compare the differences and try to think why your code reports different outputs.
