#ifndef __MEM_TRACE_SIMPLE_H__
#define __MEM_TRACE_SIMPLE_H__

#include <iostream>
#include <fstream>
#include <random>
#include <assert.h>
#include <stdlib.h> 
#include <deque>
#include <float.h>
#include "pin.H"

static uint64_t InterCount = 0;
static uint64_t NumMemAccs = 0;
static uint64_t BBVInsts = 0;
static uint64_t NumIntervals = 0;
static uint64_t IntervalSize = 0;

/* parse the command line arguments */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "BBV.txt", "specify output file name");
KNOB<UINT64> KnobAccumTabSize(KNOB_MODE_WRITEONCE, "pintool", "m", "32", "the accumulator table size");
KNOB<UINT64> KnobIntervalSize(KNOB_MODE_WRITEONCE, "pintool", "i", "10000000", "the interval size");
KNOB<UINT32> KnobRdvThreshold(KNOB_MODE_WRITEONCE, "pintool", "t", "4", "the maximum normalized manhattan distance of two RD vector");

/* for recording distribution into a Histogram, 
   Accur is the accuracy of transforming calculation */
template <class B = int64_t>
class Histogram
{
    B * bins;
    int _size;

public:
    B samples;

    Histogram() : bins(nullptr), _size(0), samples(0) {};

    Histogram(int s);

    Histogram(const Histogram<B> & rhs);

    ~Histogram();

    void setSize(int s);

    const int size() const;

    void clear();

    void normalize();

    double manhattanDist(const Histogram<B> & rhs);

    B & operator[](const int idx);

    Histogram<B> & operator=(const Histogram<B> & rhs);

    Histogram<B> & operator+=(const Histogram<B> & rhs);

    void sample(uint32_t x, int num = 1);

    const B getSamples() const; 

    void print(std::ofstream & file);
};


// This function is called before every instruction is executed
VOID PIN_FAST_ANALYSIS_CALL 
doCount(ADDRINT, BOOL, BOOL, BOOL, BOOL);

/*
 * Insert code to write data to a thread-specific buffer for instructions
 * that access memory.
 */
VOID Trace(TRACE trace, VOID *v);

/* output the results, and free the poiters */
VOID Fini(INT32 code, VOID *v);

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
INT32 Usage();

/* global variates */
std::ofstream fout;
Histogram<> currBBV;

#endif