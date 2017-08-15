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
KNOB<UINT64> KnobAccumTabSize(KNOB_MODE_WRITEONCE, "pintool", "m", "16", "the accumulator table size");
KNOB<UINT64> KnobSignTabSize(KNOB_MODE_WRITEONCE, "pintool", "m", "32", "the accumulator table size");
KNOB<UINT64> KnobIntervalSize(KNOB_MODE_WRITEONCE, "pintool", "i", "10000000", "the interval size");
KNOB<UINT32> KnobRdvThreshold(KNOB_MODE_WRITEONCE, "pintool", "t", "40", "the maximum normalized manhattan distance of two RD vector");

/* for recording distribution into a Histogram, 
   Accur is the accuracy of transforming calculation */
template <class B = uint32_t>
class Histogram
{
protected:
    B * bins;
    uint32_t _size;
    B samples;

public:
    Histogram() : bins(nullptr), _size(0), samples(0) {};

    Histogram(const uint32_t s);

    Histogram(const Histogram<B> & rhs);

    ~Histogram();

    void setSize(const uint32_t s);

    const uint32_t size() const;

    void clear();

    void normalize();

    B & operator[](const int idx);

    Histogram<B> & operator=(const Histogram<B> & rhs);

    Histogram<B> & operator+=(const Histogram<B> & rhs);

    void sample(uint32_t x, int num = 1);

    const B getSamples() const; 

    void print(std::ofstream & file);
};

template<class B = uint32_t>
class AccumulatorTable : public Histogram<B>
{
    friend class SignatureTable;
    uint32_t id;

public:
    AccumulatorTable(const int s);

    ~AccumulatorTable() {};

    void compress(Histogram<B> & hist);

    double manhattanDist(Histogram<B> & rhs);

    void setId(const uint32_t i);
};

class SignatureTable
{
private:
    /* deque for LRU replacement policy */
    std::deque<AccumulatorTable<uint32_t> *> st;
    double threshold;

public:
    SignatureTable() {};

    ~SignatureTable();

    void setThreshold(double t) { threshold = t; }

    uint32_t find(AccumulatorTable<> * & accu);

    const int size() const { return st.size(); }
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
SignatureTable signTable;

#endif