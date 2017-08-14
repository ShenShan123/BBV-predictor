/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include "bbvTrace.h"
#include "random_matrix.h"

template <class B>
Histogram<B>::Histogram(int s) : _size(s), samples(0)
{
    bins = new B[_size];
    /* init bins to 0 */
    for (int i = 0; i < _size; ++i)
        bins[i] = 0;
}

template <class B>
Histogram<B>::Histogram(const Histogram<B> & rhs) : _size(rhs._size), samples(rhs.samples)
{
    bins = new B[_size];
    for (int i = 0; i < _size; ++i)
        bins[i] = rhs.bins[i];
}

template <class B>
Histogram<B>::~Histogram() { delete [] bins; }

template <class B>
void Histogram<B>::setSize(int s)
{
    if (bins != nullptr)
        delete [] bins;

    _size = s;
    bins = new B[_size];

    /* init bins to 0 */
    for (int i = 0; i < _size; ++i)
        bins[i] = 0;

    //std::cout << "size of bins " << _size << std::endl;
}


template <class B>
const int Histogram<B>::size() const { return _size; }

template <class B>
void Histogram<B>::clear()
{
    samples = 0;
    /* when clear bins, the size keeps unchanged */
    for (int i = 0; i < _size; ++i)
        bins[i] = 0;
}

template <class B>
void Histogram<B>::normalize()
{
    for (int i = 0; i < _size; ++i)
        bins[i] /= samples;
}

template <class B>
double Histogram<B>::manhattanDist(const Histogram<B> & rhs)
{
    assert(_size == rhs._size);

    B dist = 0;
    for (uint32_t i = 0; i < _size; ++i)
        dist += std::abs(bins[i] - rhs.bins[i]);

    return (double)dist / samples;
}


template <class B>
B & Histogram<B>::operator[](const int idx)
{
    assert(idx >= 0 && idx < _size);
    return bins[idx];
}
        
template <class B>
Histogram<B> & Histogram<B>::operator=(const Histogram<B> & rhs)
{
    assert(_size == rhs.size());

    for (int i = 0; i < _size; ++i)
        bins[i] = rhs.bins[i];

    samples = rhs.samples;
    return *this;
}

template <class B>
Histogram<B> & Histogram<B>::operator+=(const Histogram<B> & rhs)
{
    assert(_size == rhs.size());

    for (int i = 0; i < _size; ++i)
        bins[i] += rhs.bins[i];

    samples += rhs.samples;
    return *this;
}

template <class B>
void Histogram<B>::sample(uint32_t x, int num)
{
    /* the sample number must less than max size of bins */
    assert(x < _size && x >= 0);

    bins[x] += num;
    /* calculate the total num of sampling */
    ++samples;
}

template <class B>
void Histogram<B>::print(std::ofstream & file)
{
    //file.write((char *)bins, sizeof(B) * _size);
    for (int i = 0; i < _size; ++i)
        file << bins[i] << " ";
    file  << "\n";
}


AccumulatorTable::Entry::Entry(const Histogram<> & rdv, const uint32_t idx) : phaseBBV(rdv), id(-1), occur(0), reuse(0), reuseIdx(idx)
{
    for (int i = 0; i < phaseBBV.size(); ++i)
        id ^= phaseBBV[i];
}

AccumulatorTable::~AccumulatorTable() 
{
    for (auto it = pt.begin(); it != pt.end(); ++it)
        delete *it;
}

uint32_t AccumulatorTable::find(const Histogram<> & rdv)
{
    ++index;
    double distMin = DBL_MAX;
    Entry * entryPtr = nullptr;

    /* search for a similar RDV of a phase */
    for (auto it = pt.begin(); it != pt.end(); ++it) {
        double dist = (*it)->phaseBBV.manhattanDist(rdv);

        if (dist < distMin) {
            distMin = dist;
            entryPtr = *it;
        }
    }

    if (distMin < threshold && entryPtr != nullptr) {
        ++entryPtr->occur;
        entryPtr->reuse = index - entryPtr->reuseIdx;
        entryPtr->reuseIdx = index;
        std::cout << "found a similar phase: id " << entryPtr->id << std::endl;
        return entryPtr->id;
    }
    else {
        Entry * newEntry = new Entry(rdv, index);
        std::cout << "creat a new phase: id " << newEntry->id << std::endl;
        pt.push_back(newEntry);
        return 0;
    }
}

// This function is called before every instruction is executed
VOID PIN_FAST_ANALYSIS_CALL
doCount(ADDRINT pc, BOOL isBranch, BOOL isMemRead, BOOL isMemWrite, BOOL hasRead2)
{
    ++BBVInsts;
    /* count the interval length */
    if (isMemRead || isMemWrite || hasRead2) {
        NumMemAccs += (uint32_t)isMemRead + isMemWrite + hasRead2;
        InterCount += (uint32_t)isMemRead + isMemWrite + hasRead2;;
    }

    if (isBranch) {
        currBBV.sample((currBBV.size() - 1) & pc, BBVInsts);
        BBVInsts = 0;
    }

    if (InterCount >= IntervalSize) {
        /* compensate the residual of insts */
        InterCount -= IntervalSize;
        ++NumIntervals;

        Histogram<double> accuTable(KnobAccumTabSize.Value());

        /* compressing BBV, decrease the dimensions */
        for(int i = 0; i < accuTable.size(); ++i) {
            for (int j = 0; j < currBBV.size(); ++j)
                accuTable[i] += (double)currBBV[j] * randM[i][j];
            accuTable.samples += std::abs(accuTable[i]);
        }

        currBBV.clear();
        accuTable.normalize();
        /* print compressed BBV */
        accuTable.print(fout);
        std::cout << "==== " << NumIntervals << "th interval ====" << std::endl;
    }

    /* if we got a maximum memory references, just exit this program */
    //if (NumMemAccs >= 50000000000) {
        //Fini(0, a);
        //exit (0);
    //}
}

/*
 * Insert code to write data to a thread-specific buffer for instructions
 * that access memory.
 */
VOID Trace(TRACE trace, VOID * v)
//VOID PIN_FAST_ANALYSIS_CALL Instruction(INS ins, VOID *v)
{
    // Insert a call to record the effective address.
    for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl=BBL_Next(bbl))
    {
        for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins=INS_Next(ins))
        {
            /* will be call for every inst */
            INS_InsertCall(
            ins, IPOINT_BEFORE,
            (AFUNPTR)doCount, IARG_FAST_ANALYSIS_CALL,
            IARG_INST_PTR, 
            IARG_BOOL, INS_IsCall(ins) || INS_IsBranch(ins) || INS_IsRet(ins),
            IARG_BOOL, INS_IsMemoryRead(ins),
            IARG_BOOL, INS_IsMemoryWrite(ins),
            IARG_BOOL, INS_HasMemoryRead2(ins),
            IARG_END);
        }
    }
}

/* output the results, and free the poiters */
VOID Fini(INT32 code, VOID *v)
{
    /* sum the current SDD to total SDD */
    //totalSDD += currBBV;
    //totalSDD->print(fout);
    ++NumIntervals;

    fout.close();

    //std::cout << "phase table size " << phaseTable.size() << std::endl;
    std::cout << "Total memory accesses " << NumMemAccs << std::endl;
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR( "This Pintool calculate the stack distance of a program.\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    IntervalSize = KnobIntervalSize.Value();
    /* out put file open */
    fout.open(KnobOutputFile.Value().c_str(), std::ios::out | std::ios::binary);
    if (fout.fail()) {
         PIN_ERROR( "output file: " + KnobOutputFile.Value() + " cannot be opened.\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
         return -1;
    }

    /* current phase BBV, max size is 2756 */
    currBBV.setSize(2756);
    //phaseTable.setThreshold((double)1 / KnobRdvThreshold.Value());

    std::cout << "truncation distance " << currBBV.size() << "\nout file " << KnobOutputFile.Value().c_str() \
    << "\ninterval size " << IntervalSize << "\nPhase threshold " << (double) 1 / KnobRdvThreshold.Value() << std::endl;

    // add an instrumentation function
    TRACE_AddInstrumentFunction(Trace, 0);

    /* when the instrucments finish, call this API */
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
