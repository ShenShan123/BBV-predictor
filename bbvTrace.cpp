/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include "bbvTrace.h"
#include "random_matrix.h"

template <class B>
Histogram<B>::Histogram(const uint32_t s) : _size(s), samples(0)
{
    bins = new B[_size];
    /* init bins to 0 */
    for (uint32_t i = 0; i < _size; ++i)
        bins[i] = 0;
}

template <class B>
Histogram<B>::Histogram(const Histogram<B> & rhs) : _size(rhs._size), samples(rhs.samples)
{
    bins = new B[_size];
    for (uint32_t i = 0; i < _size; ++i)
        bins[i] = rhs.bins[i];
}

template <class B>
Histogram<B>::~Histogram() { delete [] bins; }

template <class B>
void Histogram<B>::setSize(const uint32_t s)
{
    if (bins != nullptr)
        delete [] bins;

    _size = s;
    bins = new B[_size];

    /* init bins to 0 */
    for (uint32_t i = 0; i < _size; ++i)
        bins[i] = 0;

    //std::cout << "size of bins " << _size << std::endl;
}


template <class B>
const uint32_t Histogram<B>::size() const { return _size; }

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

inline uint8_t _bsr_int32_(uint32_t num) {
    uint8_t count = 0;
    //__asm__(
            //"bsrl %1, %0\n\t"//bsr和mov后面的l是指4字节数据宽度,
            //"jnz 1f\n\t"
            //"movl $-1,%0\n\t"
            //"1:"
            //:"=r"(count):"r"(num));
    while (num) {
        num >>= 1;
        ++count;
    }

    return count;
}

template<class B>
AccumulatorTable<B>::AccumulatorTable(const int s) : Histogram<B>(s), id(0) 
{};

template<class B>
void AccumulatorTable<B>::compress(Histogram<B> & hist)
{
    /* compressing BBV, decrease the dimensions */
    for(int i = 0; i < this->_size; ++i) {
        for (int j = 0; j < hist.size(); ++j)
            this->bins[i] += std::round((double)hist[j] * randM[i][j]);
        this->samples += std::abs(this->bins[i]);
    }

    /* get the most significant bit index of average number */
    uint32_t avg = this->samples / this->_size;
    uint8_t mostSignBitIdx = _bsr_int32_((uint32_t)avg);
    this->samples = 0;

    for (int i = 0; i < this->_size; ++i) {
        /* if the bin value lager than the 2*average, we set all 6 bits to 1 */
        if (this->bins[i] > 2 * avg)
            this->bins[i] = 0x3f;
        /* we total keep 6bits, 2 bits of each bin which is corresponding with 2-MSBs of the average number, as well as 4-lower bits*/
        assert(mostSignBitIdx >= 4);
        this->bins[i] = (this->bins[i] >> (mostSignBitIdx - 4)) & 0x3f;
        this->samples += this->bins[i];
    }
}

template<class B>
double AccumulatorTable<B>::manhattanDist(Histogram<B> & rhs)
{
    assert(this->_size == rhs.size());

    B dist = 0;
    for (uint32_t i = 0; i < this->_size; ++i)
        /* mahattan(a, b) = sum(|ai-bi|) */
        dist += std::abs(this->bins[i] - rhs[i]);

    return (double)dist / this->samples;
}

template<class B>
void AccumulatorTable<B>::setId(const uint32_t i) { id = i; }


SignatureTable::~SignatureTable()
{
    for (auto it = st.begin(); it != st.end(); ++it)
        delete *it;
}

uint32_t SignatureTable::find(AccumulatorTable<> * & accu)
{
    double distMin = DBL_MAX;
    AccumulatorTable<uint32_t> * entryPtr = nullptr;

    /* search for a similar RDV of a phase */
    for (auto it = st.begin(); it != st.end(); ++it) {
        double dist = (*it)->manhattanDist(*accu);

        if (dist < distMin) {
            distMin = dist;
            entryPtr = *it;
        }
    }

    /* if found a similar entry in signature table, just delete the accu */
    if (distMin < threshold && entryPtr != nullptr) {
        delete accu;
        std::cout << "found a similar phase: id " << entryPtr->id << std::endl;
        return entryPtr->id;
    }
    else {
        entryPtr = accu;
        entryPtr->setId(st.size() + 1);
        st.push_back(entryPtr);
        std::cout << "create a new phase: id " << entryPtr->id << std::endl;
        return entryPtr->id;
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
        InterCount += (uint32_t)isMemRead + isMemWrite + hasRead2;
    }

    if (isBranch) {
        currBBV.sample((currBBV.size() - 1) & pc, BBVInsts);
        BBVInsts = 0;
    }

    if (InterCount >= IntervalSize) {
        /* compensate the residual of insts */
        InterCount -= IntervalSize;
        ++NumIntervals;

        AccumulatorTable<> * accuTable = new AccumulatorTable<>(KnobAccumTabSize.Value());
        accuTable->compress(currBBV);
        accuTable->print(fout);
        signTable.find(accuTable);

        currBBV.clear();
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
    signTable.setThreshold((double)KnobRdvThreshold.Value() / 100);

    std::cout << "truncation distance " << currBBV.size() << "\nout file " << KnobOutputFile.Value().c_str() \
    << "\ninterval size " << IntervalSize << "\nPhase threshold " << (double) KnobRdvThreshold.Value() / 100 \
    << "\naccumulate table size " << KnobAccumTabSize.Value() << std::endl;

    // add an instrumentation function
    TRACE_AddInstrumentFunction(Trace, 0);

    /* when the instrucments finish, call this API */
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
