# Operating Systems Project 4  
**CS3113 – Introduction to Operating Systems – Spring 2025**

## Project Description  
This project simulates a segmented memory management system as part of a broader operating system process management simulation. It extends the previous projects by implementing logical-to-physical memory address translation through the use of a segment table. Each process is allocated non-contiguous memory segments, and address translation is dynamically performed based on segment information during execution.

Processes include both a segment table and PCB fields stored in logical memory. Memory coalescing is used to optimize free space utilization when necessary.

## Key Enhancements  
1. **Segmented Memory Management**  
   - Each process can occupy up to six non-contiguous segments.
   - Logical addresses are translated into physical addresses using the segment table.

2. **Segment Table Storage**  
   - Each process has a segment table stored contiguously in memory (minimum of 13 integers).

3. **Logical-to-Physical Address Translation**  
   - CPU instructions dynamically translate logical addresses at runtime, validating bounds.

4. **Dynamic Memory Management with Coalescing**  
   - Free memory blocks are merged when possible to minimize fragmentation.

5. **Detailed Logging and Validation**  
   - Logical and physical address translations are logged.
   - Memory violations are detected and reported.

## Files Included  
- `CS3113_Project4.cpp`: Full C++ implementation of segmented memory simulation.  
- `DETAILS.pdf`: Official project specification document.  
- `README.md`: Documentation for project overview and usage instructions.

## Input Format  
The program reads from standard input (via redirection). The expected input structure is:
1. An integer specifying the maximum size of main memory  
2. CPU time slice (cycles allocated before timeout)  
3. Context switch time (in cycles)  
4. Number of processes  
