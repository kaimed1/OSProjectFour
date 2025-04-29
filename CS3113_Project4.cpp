#include <iostream>
#include <sstream>
#include <queue>
#include <string>
#include <iomanip>
#include <list>
#include <iterator>
#include <unordered_map>
#include <vector>

using namespace std;

//these are global variables
int alloccpu;       
int cst; 
int globalcpu;  
queue<int> iowaitq; 
int *mm = nullptr; 

// maps in order
unordered_map<int,int> startproc; 
unordered_map<int,int> dbmap;  
unordered_map<int,int> usedcpucycle;  
unordered_map<int,int> mustw;     

//structs
struct memn {
    int pid;     
    int startadd;   
    int size;           
    memn(int s): pid(-1), startadd(-1), size(s) {}
    memn(int p,int st,int s)  : pid(p),  startadd(st), size(s) {}
    void print() const { cout << pid << " " << startadd << " " << size << endl; }
};

list<memn> cpumem;

//helpers
void contswit() { globalcpu += cst; }

int log2phys(int logadd,int *pcb) {
    int segtabsize = pcb[0];
    int numsegs = segtabsize / 2;
    int r = logadd;
    for(int i = 0; i < numsegs; i++) {
        int s  = pcb[1 + 2 * i];
        int l = pcb[1 + 2 * i + 1];
        if(r < l) return s + r;
        r -= l;
    }
    cout << "Memory violation: address " << logadd << " out of bounds." << endl;
    return -1;
}

// forward declarations
void procIOwait(queue<int>& readyq);

//pcb structure!!
struct PCB {
    // private
    int pid;
    string statef;
    int pc;
    string instrbase;
    int dataBase;
    int memoryLimit;
    int cpuCycles;
    int regVal;
    int mmBase;


// struct PCB {
//     int processID;
//     // Process states: 0 = new, 1 = ready, 2 = running, 3 = i/o waiting, 4 = terminated
//     int state;
//     int programCounter;
//     int instructionBase;
//     int dataBase;
//     int memoryLimit;     // number of words needed (excluding PCB overhead)
//     int cpuUsed;
//     int registerValue;
//     int maxMemoryNeeded; // as given by input (e.g., for process 1: 231)
//     int mainMemoryBase;  // assigned start address in mainMemory
//     vector<int> logicalMemory; // instructions and associated data
//     // Segment table: Two segments (Instructions and Data)
//     struct Segment {
//         int base;  // Physical base address of the segment
//         int limit; // Size of the segment
//     };
//     Segment segmentTable[2]; // Segment 0: Instructions, Segment 1: Data
// };

public:
    int maxMemoryNeeded;

    PCB() : pid(0), statef("NEW"), pc(0), instrbase(""),
             dataBase(0), memoryLimit(0), cpuCycles(0), regVal(0), mmBase(0),
             maxMemoryNeeded(0) {}

    PCB(int pid,int maxMem,const string &instr)
        : pid(pid), statef("NEW"), pc(0), instrbase(instr),
          dataBase(0), memoryLimit(maxMem), cpuCycles(0), regVal(0), mmBase(0),
          maxMemoryNeeded(maxMem) {}

    void setDataBase(int db) { dataBase = db; }
    void setmmBase(int base) { mmBase = base; }
//spaced out for visual
    // getters
    int getpid()const
    { return pid; }
    int maxmemneed()const
    { return maxMemoryNeeded; }
    int getpc()const 
    { return pc; }
    int getDataBase()const
    { return dataBase; }
    int getMemoryLimit()const 
    { return memoryLimit; }
    int getusedcpucycle()const 
    { return cpuCycles; }
    int getRegister()const 
    { return regVal; }
    int getmmBase()const 
    { return mmBase; }
    const string &getInstructionBase() const 
    { return instrbase; }
};


void processtomem(int *processLogical,int totalSize,int *pcb){
    int segtabsize = pcb[0];
    int numsegs      = segtabsize / 2;
    int logind = 0;
    for(int i = 0; i < numsegs; i++){
        int s = pcb[1 + 2 * i];
        int size  = pcb[1 + 2 * i + 1];
        for(int j = 0; j < size && logind < totalSize; j++)
            mm[s + j] = processLogical[logind++];
    }
    if(logind < totalSize)
        cout << "Error: not enough space in allocated segs to hold process." << endl;
}

vector<int> buildlogmem(PCB &pcb,int segStart,int numsegs){
    vector<int> arr;
    istringstream stream(pcb.getInstructionBase());

    // first 10 PCB
    arr = { pcb.getpid(), 1, 0, numsegs * 2 + 11, 0, pcb.maxmemneed(),
            0, 0, pcb.maxmemneed(), segStart };
    
    vector<int> instrArr, dataa;
    int numInstr; stream >> numInstr;
    int pc = segStart + 10;
    int db = pc + numInstr;

    for(int i = 0; i < numInstr; i++){
        int op; stream >> op; instrArr.push_back(op);
        switch(op){
            case 1:{int a,b;stream >> a >> b; dataa.push_back(a); dataa.push_back(b); db += 2; break;}
            case 2:{int a;stream >> a; dataa.push_back(a); db++; break;}
            case 3:{int a,b;stream >> a >> b; dataa.push_back(a); dataa.push_back(b); db += 2; break;}
            case 4:{int a;stream >> a; dataa.push_back(a); db++; break;}
            default: cout << "unexpected operation number in loadJobtoMemory" << endl;
        }
        pc++;
    }
    instrArr.insert(instrArr.end(), dataa.begin(), dataa.end());
    arr.insert(arr.end(), instrArr.begin(), instrArr.end());
    arr[4] = 2 * numsegs + 11 + numInstr;
    return arr;
}

void storeSegmentTable(int segStart,int numsegs,int **segs,PCB &pcb){
    int idx = segStart;
    mm[idx++] = numsegs * 2;            // size of segment table
    for(int i = 0; i < numsegs; i++){           // physical base/size pairs
        mm[idx++] = segs[i][0];
        mm[idx++] = segs[i][1];
    }

    int logind = 0;
    int total = pcb.maxmemneed() + 23;
    int s = idx;
    int size  = segs[0][1] - (numsegs * 2 + 1);
    vector<int> logicalMem = buildlogmem(pcb, segStart, numsegs);

    for(int seg = 1; seg <= numsegs && logind < (int)logicalMem.size(); seg++){
        for(int j = 0; j < size && logind < (int)logicalMem.size(); j++)
            mm[s + j] = logicalMem[logind++];
        if(seg > 5) break;
        s = segs[seg][0];
        size  = segs[seg][1];
    }
}

//coalesce free memory blocks
bool freecoal(){
    bool merged = false;
    for(auto it = cpumem.begin(); it != cpumem.end();) {
        auto nextIt = next(it);
        if(nextIt != cpumem.end() && it->pid == -1 && nextIt->pid == -1){
            int newSize = it->size + nextIt->size;
            int s = it->startadd;
            it = cpumem.erase(it);
            it = cpumem.erase(it);
            it = cpumem.insert(it, memn(-1, s, newSize));
            merged = true;
        } else { ++it; }
    }
    return merged;
}

// allocate memory for multi-segment process
bool allocateMultiSegment(queue<PCB>& newJob, queue<int>& ready){
    if(newJob.empty()) return false;
    PCB &job = newJob.front();
    freecoal();

    auto it = cpumem.begin();
    int allocatedsegs = 0;
    int memNeeded = job.maxmemneed() + 23;
    int **segs = new int*[6]; for(int i = 0; i < 6; i++) segs[i] = new int[2];
    int idx = 0;

    // find first block ≥13 words for segment table
    while(it != cpumem.end() && allocatedsegs < 6 && memNeeded > 0){
        if(it->pid == -1 && it->size >= 13){
            segs[idx][0] = it->startadd; segs[idx][1] = it->size;
            memNeeded -= it->size; allocatedsegs++; idx++; ++it;
            while(memNeeded > 0 && it != cpumem.end() && allocatedsegs < 6){
                if(it->pid == -1){
                    segs[idx][0] = it->startadd; segs[idx][1] = it->size;
                    memNeeded -= it->size; allocatedsegs++; idx++;
                }
                ++it;
            }
            break;
        }
        ++it;
        // if(it == cpumem.end()) break;
        if(it == cpumem.end()){
            cout << "Process " << job.getpid() << " could not be loaded due to insufficient contiguous space for segment table." << endl;
            for(int i = 0; i < 6; i++) delete[] segs[i]; delete[] segs; return false;
        }
    }

    if(memNeeded > 0){
        cout << "Insufficient memory for Process " << job.getpid() << ". Attempting memory coalescing." << endl;
        cout << "Process " << job.getpid() << " waiting in NewJobQueue due to insufficient memory." << endl;
        for(int i = 0; i < 6; i++) delete[] segs[i]; delete[] segs; return false;
    }

    // allocate blocks
    int sizeAccum = 0;
    for(int s = 0; s < allocatedsegs; s++){
        for(it = cpumem.begin(); it != cpumem.end(); ++it){
            if(it->startadd == segs[s][0]){ it->pid = job.getpid(); sizeAccum += it->size; break; }
        }
    }
    int trimSize = sizeAccum - (job.maxmemneed() + 23);
    segs[allocatedsegs - 1][1] -= trimSize;

    cout << "Process " << job.getpid() << " loaded with segment table stored at physical address " << segs[0][0] << endl;
    storeSegmentTable(segs[0][0], allocatedsegs, segs, job);

    // split last block to free leftover
    for(it = cpumem.begin(); it != cpumem.end(); ++it){
        if(it->startadd == segs[allocatedsegs - 1][0]){
            memn procNode(job.getpid(), it->startadd, segs[allocatedsegs - 1][1]);
            memn freeNode(-1, procNode.startadd + procNode.size, it->size - procNode.size);
            cpumem.insert(it, procNode);
            cpumem.insert(it, freeNode);
            it = cpumem.erase(it);
            break;
        }
    }
    // update main memory
    ready.push(segs[0][0]);
    newJob.pop();
    for(int i = 0; i < 6; i++) delete[] segs[i]; delete[] segs;
    return true;
}

// single‑segment fit helper
bool allocateSingleSegment(PCB job, queue<int>& ready){
    for(auto it = cpumem.begin(); it != cpumem.end(); ++it){
        if(it->pid == -1 && job.maxmemneed() + 23 <= it->size){
            memn procNode(job.getpid(), it->startadd, job.maxmemneed() + 23);
            memn freeNode(-1, it->startadd + procNode.size, it->size - procNode.size);
            cpumem.insert(it, procNode);
            cpumem.insert(it, freeNode);
            it = cpumem.erase(it);
            cout << "Process " << procNode.pid << " loaded with segment table stored at physical address " << procNode.startadd << endl;
            ready.push(job.getmmBase());
            return true;
        }
    }
    cout << "Insufficient memory for Process " << job.getpid() << ". Attempting memory coalescing." << endl;
    cout << "Process " << job.getpid() << " waiting in jobq due to insufficient memory." << endl;
    return false;
}

//loadJobsToMemory
void loadJobsToMemory(queue<PCB>& newJobs, queue<int>& ready){
    while(!newJobs.empty()){
        if(!allocateMultiSegment(newJobs, ready)) break;
    }
}

// free memory for terminated process
void freeMemory(list<memn>& mem,int startAddr,int pcb[]){
    for(auto &blk : mem) if(blk.pid == mm[startAddr]) blk.pid = -1;
    int segtabsize = pcb[0]; int numsegs = segtabsize / 2;
    for(int seg = 0; seg < numsegs; seg++){
        int s = pcb[1 + 2 * seg]; int size = pcb[2 + 2 * seg];
        for(int j = 0; j < size; j++) mm[s + j] = -1;
    }
}

// free memory for terminated process
void procIOwait(queue<int>& readyq){
    queue<int> temp;
    while(!iowaitq.empty()){
        int pcbadd = iowaitq.front();
        int segtabsize = mm[pcbadd];
        int pcbarr[segtabsize + 1];
        for(int i = 0; i <= segtabsize; i++) pcbarr[i] = mm[pcbadd + i];
        int pidadd = log2phys(segtabsize + 1, pcbarr);
        int pcAddress = log2phys(segtabsize + 3, pcbarr);
        int dbadd = log2phys(segtabsize + 5, pcbarr);
        int pid = mm[pidadd];
        int cycneed = mm[log2phys(mm[dbadd], pcbarr)];
        if(globalcpu - mustw[pid] >= cycneed){
            usedcpucycle[pid] += cycneed;
            mm[dbadd]++; // advance over cycle count
            mm[pcAddress]++; // next instruction
            readyq.push(iowaitq.front());
            cout << "print" << endl;
            cout << "Process " << pid << " completed I/O and is moved to the ReadyQueue." << endl;
            iowaitq.pop();
        } else {
            temp.push(iowaitq.front()); iowaitq.pop();
        }
    }
    iowaitq = temp;
}

// executecpu
void executeCPU(int pcbadd, queue<int>& readyq, queue<PCB>& jobq) {
    int segtabsize = mm[pcbadd];
    int pcbarr[segtabsize + 1];
    for (int i = 0; i <= segtabsize; i++) {
        pcbarr[i] = mm[pcbadd + i];
    }

    int pidadd = log2phys(segtabsize + 1, pcbarr);
    int stateadd = log2phys(segtabsize + 2, pcbarr);
    int pcAddress = log2phys(segtabsize + 3, pcbarr);
    int instrbadd = log2phys(segtabsize + 4, pcbarr);
    int dbadd = log2phys(segtabsize + 5, pcbarr);
    int memlim = log2phys(segtabsize + 6, pcbarr);
    int cpuUsed = log2phys(segtabsize + 7, pcbarr);
    int regAdd = log2phys(segtabsize + 8, pcbarr);
    int maxMem = log2phys(segtabsize + 9, pcbarr);
    int mmBAdd = log2phys(segtabsize + 10, pcbarr);
    int pid = mm[pidadd];
    if (usedcpucycle.find(pid) == usedcpucycle.end()) {
        usedcpucycle[pid] = 0;
    }
    if (dbmap.find(pid) == dbmap.end()) {
        dbmap[pid] = mm[dbadd];
    }

    contswit();
    if (startproc.find(pid) == startproc.end()) {
        startproc[pid] = globalcpu;
    }
    int startclock = globalcpu;

    cout << "Process " << pid << " has moved to Running." << endl;

    while (mm[log2phys(mm[dbadd], pcbarr)] != -1) {
        int instruction = mm[log2phys(mm[pcAddress] + mm[instrbadd], pcbarr)];
        switch(instruction){
            case 1:{ // compute
                cout << "compute" << endl;
                mm[dbadd]++;
                int cycles = mm[log2phys(mm[dbadd], pcbarr)];
                usedcpucycle[pid] += cycles;
                globalcpu += cycles;
                mm[dbadd]++;
                break;}
            case 2:{ // IO
                mustw[pid] = globalcpu;
                cout << "Process " << pid << " issued an IOInterrupt and moved to the IOWaitingQueue." << endl;
                iowaitq.push(pcbadd);
                procIOwait(readyq);
                mm[stateadd] = 3; // I/O waiting
                return;}
            case 3:{ // store
                {
                    int value = mm[log2phys(mm[dbadd], pcbarr)]; mm[dbadd]++;
                    mm[regAdd] = value;
                    int logadd = mm[log2phys(mm[dbadd], pcbarr)]; mm[dbadd]++;
                    int physadd = log2phys(logadd, pcbarr);
                    if(logadd < mm[memlim]){ cout << "stored" << endl; mm[physadd] = value; }
                    else { cout << "store error!" << endl; }
                    cout << "Logical address " << logadd << " translated to physical address " << physadd << " for Process " << pid << endl;
                    usedcpucycle[pid]++; globalcpu++;
                }
                break;}
            case 4:{ // load
                {
                    int logadd = mm[log2phys(mm[dbadd], pcbarr)]; mm[dbadd]++;
                    int physadd = log2phys(logadd, pcbarr);
                    cout << "loaded" << endl;
                    cout << "Logical address " << logadd << " translated to physical address " << physadd << " for Process " << pid << endl;
                    if(logadd < mm[memlim]) mm[regAdd] = mm[physadd];
                    else { cout << "load error!" << endl; mm[regAdd] = -1; }
                    usedcpucycle[pid]++; globalcpu++;
                }
                break;}
            default:
                cout << "ERROR: unexpected instruction: " << instruction << endl; return;
        }
        mm[pcAddress]++;
        if(globalcpu - startclock >= alloccpu && mm[log2phys(mm[dbadd], pcbarr)] != -1){
            cout << "Process " << pid << " has a TimeOUT interrupt and is moved to the ReadyQueue." << endl;
            readyq.push(pcbadd); procIOwait(readyq); return;
        }
    }

    // termination
    cout << "Process ID: " << pid << endl;
    cout << "State: TERMINATED" << endl;
    cout << "Program Counter: " << mm[instrbadd] - 1 << endl;
    cout << "Instruction Base: " << mm[instrbadd] << endl;
    cout << "Data Base: " << dbmap[pid] << endl;
    cout << "Memory Limit: " << mm[memlim] << endl;
    cout << "CPU Cycles Used: " << usedcpucycle[pid] << endl;
    cout << "Register Value: " << mm[regAdd] << endl;
    cout << "Max Memory Needed: " << mm[maxMem] << endl;
    cout << "Main Memory Base: " << mm[mmBAdd] << endl;
    cout << "Total CPU Cycles Consumed: " << globalcpu - startproc[pid] << endl;
    cout << "Process " << pid << " terminated. Entered running state at: " << startproc[pid]
        << ". Terminated at: " << globalcpu << ". Total Execution Time: "
        << globalcpu - startproc[pid] << "." << endl;
    cout << "Process " << pid << " terminated and freed memory blocks." << endl;
    freeMemory(cpumem, pcbadd + segtabsize + 1, pcbarr);

    while(!jobq.empty() && allocateMultiSegment(jobq, readyq));
    procIOwait(readyq);
}

//main function
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int mmLimit, numproc;
    cin >> mmLimit >> alloccpu >> cst >> numproc;

    mm = new int[mmLimit];
    for(int i = 0; i < mmLimit; i++) mm[i] = -1;
    cpumem.push_front(memn(-1, 0, mmLimit));

    queue<PCB> jobq; queue<int> readyq;

    string line; getline(cin, line); // consume endline
    for(int p = 0; p < numproc; p++){
        getline(cin, line);
        istringstream ss(line);
        int pid, maxMem; ss >> pid >> maxMem; ss.get();
        string instrBase = ss.str().substr(ss.tellg());
        jobq.emplace(pid, maxMem, instrBase);
    }

    loadJobsToMemory(jobq, readyq);

    // print memory snapshot
    for(int i = 0; i < mmLimit; i++) cout << i << " : " << mm[i] << endl;

    while(!readyq.empty() || !iowaitq.empty()){
        if(!readyq.empty()){
            int pcbadd = readyq.front(); readyq.pop(); executeCPU(pcbadd, readyq, jobq);
        } else {
            contswit(); procIOwait(readyq);
        }
    }
    // print final memory snapshot
    contswit();
    cout << "Total CPU time used: " << globalcpu << ".";
    delete[] mm;
    return 0;
}