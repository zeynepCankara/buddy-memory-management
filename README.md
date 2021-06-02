# Main Memory Management Library with Buddy Allocation

Implementation of a main memory management library with [buddy memory allocation algorithm](https://en.wikipedia.org/wiki/Buddy_memory_allocation)

## Contents

- sbmem.h: is the interface (header file) for the Library.
- sbmemlib.c: is the implementation of the library
- Makefile
- test.c (contains the experiments for the report)
- report.pdf (contains experimental results and implementation details)
- Project3.pdf (project description)

## How to Run

- cd to the project directory.
- compilation and linking done by:

```
$ make
```

##### Recompile

```
$ make clean
$ make
```

##### Running the program

```
$ ./create_memory_sb
$ ./app
$ ./destroy_memory_sb
```

##### Running the test (experimentation)

```
$ ./test
```