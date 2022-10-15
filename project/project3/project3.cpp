#include <iostream>
#include <vector>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <algorithm>

using std::cout;
using std::cin;
using std::endl;
using std::vector;

//page and frame 1 = 32bytes
struct pageTable{
    bool Vbit = false;
    int frameIdx = -1;
    int pageIdx = -1;
    int size;
    int AID;
};
//each ptable's index is AID
struct Process{
    bool* isempty;
    vector<pageTable> ptable;
    int ntable = 0;
};
//for physics memory
struct Frame{
    int AID;
    int PID;
    bool refer = true;  //for sampled LRU
    bool use = false;   //for second chance 
    int count = 1;      //for LFU, MFU
    int rbyte = 256;    //for sampled LRU
};
//for buddy system
struct Buddy{
    int AID = -1;       //if not allocating
    int PID = -1;       //if not allocating
    bool use = false;   //if not allocating
    int size;
    int addr;
};
//for stack LRU
struct miniF{
    int AID;
    int PID;
};

//for divide replace mode
enum pageAlgo{FIFO, STACK, SAMPLED, SECCHANCE, LFU, MFU, OPTI, ERR};
pageAlgo mode;

//for memory control
vector<Process> process;
vector<Buddy> buddysys;
vector<Frame> physframe;
vector<miniF> stack;
int clockHand = 0;

//grobal variable
int pageFault = 0;
//number of instruction, number of process, virtual memory size, physcal memory size
int ninstr, nProc, virmem, phymem, runinstr;
//instruction variable
int** instr;

//fifo replace algorithm
void fifo(int &rAID, int &rPID){
    rAID = physframe.front().AID;
    rPID = physframe.front().PID;
}
//stack LRU replace algorithm
void stackLRU(int &rAID, int &rPID){
    vector<miniF>::iterator i = stack.begin();
    rAID = i->AID;
    rPID = i->PID;
}
//sampled LRU replace algorithm
void sampled(int &rAID, int &rPID){
    int cpbyte = INT_MAX;
    vector<Frame>::iterator i;
    for(i = physframe.begin(); i != physframe.end(); i++){
        if(i->rbyte < cpbyte){
            rAID = i->AID;
            rPID = i->PID;
            cpbyte = i->rbyte;
        }
    }
}
//second chance replace algorithm
void secondChance(int &rAID, int &rPID){
    if(clockHand >= physframe.size()) clockHand = 0;
    while(true){
        if(!physframe[clockHand].use){
            rAID = physframe[clockHand].AID;
            rPID = physframe[clockHand].PID;
            break;
        }
        else physframe[clockHand].use = false;
        clockHand++;
        if(clockHand >= physframe.size()) clockHand = 0;
    }
}
//LFU replace algorithm
void lfu(int &rAID, int &rPID){
    vector<Frame>::iterator i;
    int cpcount = INT_MAX;
    for(i = physframe.begin(); i != physframe.end(); i++){
        if(i->count < cpcount){
            rAID = i->AID;
            rPID = i->PID;
            cpcount = i->count;
        }
    }
}
//MFU replace algorithm
void mfu(int &rAID, int &rPID){
    vector<Frame>::iterator i;
    int cpcount = 0;
    for(i = physframe.begin(); i != physframe.end(); i++){
        if(i->count > cpcount){
            rAID = i->AID;
            rPID = i->PID;
            cpcount = i->count;
        }
    }
};
//optimal replace algorithm
void optimal(int &rAID, int &rPID){
    vector<Frame>::iterator i;
    vector<int> recycle;
    bool isAccess;
    for(i = physframe.begin(); i != physframe.end(); i++){
        isAccess = false;
        for(int j = runinstr; j < ninstr; j++){
            if(instr[j][0] == 1 && instr[j][1] == i->PID && instr[j][2] == i->AID){
                recycle.push_back(j-runinstr);
                isAccess = true;
                break;
            }
        }
        if(!isAccess){
            rAID = i->AID;
            rPID = i->PID;
            return;
        }
    }
    int max_idx = max_element(recycle.begin(), recycle.end()) - recycle.begin();
    rAID = physframe[max_idx].AID;
    rPID = physframe[max_idx].PID;
};

//Returns the table index of the page corresponding to pid and aid
int getAidx(int Pidx, int Aidx){
    for(int i = 0; i < process[Pidx].ptable.size(); i++){
        if(process[Pidx].ptable[i].AID == Aidx) return i;
    }
    return -1;
}

//Returns the table index of the page corresponding to pid and virtual memory address
int getIidx(int Pidx, int idx){
    for(int i = 0; i < process[Pidx].ptable.size(); i++){
        if(process[Pidx].ptable[i].pageIdx == idx) return i;
    }
    return -1;
}

//update the reference byte
void updatebyte(){
    vector<Frame>::iterator i;
    for(i = physframe.begin(); i != physframe.end(); i++){
        i->rbyte >>= 1;
        if(i->refer) i->rbyte += 256;
        i->refer = false;
    }
}

//merge buddy
void merge(){
    vector<Buddy>::iterator i, j;
    bool isDone = true;
    while(isDone){
        isDone = false;
        for(i = buddysys.begin(); i < buddysys.end() - 1; i++){
            j = i + 1;
            if(i->size == j->size && !i->use && !j->use){
                if((i->addr / i->size) % 2 != 0) continue;
                Buddy bigger = {-1, -1, false, i->size*2, i->addr};
                buddysys.erase(i);
                buddysys.erase(i);
                buddysys.insert(i, bigger);
                isDone = true;
            }
        }
    }
}

//Remove frames corresponding to pid and aid from physical memory
void removeFrame(int rAID, int rPID){
    vector<Frame>::iterator i;
    vector<Buddy>::iterator j;
    vector<miniF>::iterator k;
    for(i = physframe.begin(); i != physframe.end(); i++){
        if(i->AID == rAID && i->PID == rPID){
            process[i->PID].ptable[getAidx(i->PID, i->AID)].frameIdx = -1;
            process[i->PID].ptable[getAidx(i->PID, i->AID)].Vbit = false;
            physframe.erase(i);
            break;
        }
    }
    for(j = buddysys.begin(); j != buddysys.end(); j++){
        if(j->AID == rAID && j->PID == rPID){
            j->AID = j->PID = -1;
            j->use = false;
        }
    }
    for(k = stack.begin(); k != stack.end(); k++){
        if(k->AID == rAID && k->PID == rPID) {
            stack.erase(k);
            break;
        }
    }
    merge();
}

//return frame address
int frameAddress(int AID, int PID){
    vector<Buddy>::iterator i;
    for(i = buddysys.begin(); i != buddysys.end(); i++){
        if(i->AID == AID && i->PID == PID) return i->addr;
    }
    return -1;
}

//return empty buddy system's frame iterator
vector<Buddy>::iterator emptyFrame(int nframe){
    int cursize = phymem;
    bool empty = false;
    vector<Buddy>::iterator i, cur;
    for(i = buddysys.begin(); i != buddysys.end(); i++){
        if(nframe <= i->size && i->size <= cursize){
            if(!i->use){
                cursize = i->size;
                cur = i;
                empty = true;
            }
        }
    }
    if(empty) return cur;
    else return buddysys.end();
}

//divide buddy fiting frame size
void makeBuddy(vector<Buddy>::iterator i, int nframe, int Aid, int Pid){
    int size = i->size;
    while(size != nframe){
        size /= 2;
        Buddy childl = {-1, -1, false, size, i->addr};
        Buddy childr = {-1, -1, false, size, i->addr + size};
        buddysys.erase(i);
        buddysys.insert(i, childr);
        i = buddysys.insert(i, childl);
    }
    i->use = true;
    i->PID = Pid;
    i->AID = Aid;
}

//virtual memory allocation
//Explore from low addresses until memory of the corresponding size exists
void memAlloc(int idx, int np){
    pageTable onetable;
    onetable.size = np;
    onetable.AID = process[idx].ntable++;
    int needs = np;
    int curidx = 0;
    for(int i = 0; i < needs; i++){
        if(!process[idx].isempty[i]){
            curidx = i + 1;
            needs = np + i + 1;
        }
    }
    for(int i = curidx; i < needs; i++) process[idx].isempty[i] = false;
    onetable.pageIdx = curidx;
    process[idx].ptable.push_back(onetable);
}

//physical memory access 
//if page is not in physical memory then load page in frame
void memAccess(int Pidx, int Aidx){
    //pase exist in frame
    if(process[Pidx].ptable[getAidx(Pidx, Aidx)].Vbit){
        vector<Frame>::iterator i;
        vector<miniF>::iterator j;
        for(i = physframe.begin(); i != physframe.end(); i++){
            if(i->PID == Pidx && i->AID == Aidx){
                i->count++;
                i->use = i->refer = true;
            }
        }
        for(j = stack.begin(); j != stack.end(); j++){
            if(j->AID == Aidx && j->PID == Pidx){
                miniF one = {Aidx, Pidx};
                stack.erase(j);
                stack.push_back(one);
            }
        }
        return;
    }
    //corresponding page does not exist in the frame
    int nframe = 1;
    //Find the size of the multiples of 2
    while(true){
        if(nframe >= process[Pidx].ptable[getAidx(Pidx, Aidx)].size) break;
        nframe <<= 1;
    }
    vector<Buddy>::iterator i = emptyFrame(nframe);
    while(i == buddysys.end()){
        int delAID, delPID;
        switch(mode){
            case FIFO:{
                fifo(delAID, delPID);
                break;
            }
            case STACK:{
                stackLRU(delAID, delPID);
                break;
            }
            case SAMPLED:{
                sampled(delAID, delPID);
                break;
            }
            case SECCHANCE:{
                secondChance(delAID, delPID);
                break;
            }
            case LFU:{
                lfu(delAID, delPID);
                break;
            }
            case MFU:{
                mfu(delAID, delPID);
                break;
            }
            case OPTI:{
                optimal(delAID, delPID);
                break;
            }
        }
        removeFrame(delAID, delPID);
        i = emptyFrame(nframe);
    }
    pageFault++;
    makeBuddy(i, nframe, Aidx, Pidx);
    Frame one = {Aidx, Pidx, true, 1, 256};
    miniF temp = {Aidx, Pidx};
    process[Pidx].ptable[getAidx(Pidx, Aidx)].Vbit = true;
    process[Pidx].ptable[getAidx(Pidx, Aidx)].frameIdx = frameAddress(Aidx, Pidx);
    stack.push_back(temp);
    physframe.push_back(one);
}

//Unassign virtual and physical memory
void memRelease(int Pidx, int Aidx){
    int delidx = process[Pidx].ptable[getAidx(Pidx, Aidx)].pageIdx;
    int delsize = process[Pidx].ptable[getAidx(Pidx, Aidx)].size;
    for(int i = delidx; i < delsize + delidx; i++) process[Pidx].isempty[i] = true;
    removeFrame(Aidx, Pidx);
    process[Pidx].ptable.erase(process[Pidx].ptable.begin() + getAidx(Pidx, Aidx));
}

//Read Main Function argument Value and Return Mode
pageAlgo readmode(int argc, char* argv){
    if(strcmp(argv, "-page=fifo") == 0) return FIFO;
    if(strcmp(argv, "-page=stack") == 0) return STACK;
    if(strcmp(argv, "-page=sampled") == 0) return SAMPLED;
    if(strcmp(argv, "-page=second-chance") == 0) return SECCHANCE;
    if(strcmp(argv, "-page=lfu") == 0) return LFU;
    if(strcmp(argv, "-page=mfu") == 0) return MFU;
    if(strcmp(argv, "-page=optimal") == 0) return OPTI;
    return ERR;
}

//Receive and save input values
void getInput(){
    cin >> ninstr >> nProc >> virmem >> phymem;
    virmem /= 32;
    phymem /= 32;
    instr = new int*[ninstr];
    for(int i = 0; i < ninstr; i++) instr[i] = new int[3];
    for(int i = 0; i < nProc; i++){
        Process oneproc;
        oneproc.isempty = new bool[virmem];
        for(int j = 0; j < virmem; j++) oneproc.isempty[j] = true;
        process.push_back(oneproc);
    }
    for(int i = 0; i < ninstr; i++){
        cin >> instr[i][0] >> instr[i][1] >> instr[i][2];
    }
}


//Outputs results to meet requirements
void printResult(){
    //structure for print output
    vector<Buddy>::iterator bi;
    vector<pageTable>::iterator pj;
    int ***printvm;
    printvm = new int**[nProc];
    for(int i = 0; i < nProc; i++) {
        printvm[i] = new int*[3];
        for(int j = 0; j < 3; j++){
            printvm[i][j] = new int[virmem];
        }
    }
    for(int i = 0; i <nProc; i++){
        for(int j = 0; j < 3; j++){
            for(int k = 0; k < virmem; k++) printvm[i][j][k] = -1;
        }
    }
    for(int i = 0; i < nProc; i++){
        for(pj = process[i].ptable.begin(); pj < process[i].ptable.end(); pj++){
            for(int j = pj->pageIdx; j < pj->pageIdx + pj->size; j++){
                printvm[i][0][j] = pj->AID;
                printvm[i][1][j] = pj->Vbit;
                printvm[i][2][j] = pj->frameIdx;
            }
        }
    }
    int doubleword = 4;
    //Physical Memory (PID)
    printf("%-30s", ">> Physical Memory (PID): ");
    for(bi = buddysys.begin(); bi != buddysys.end(); bi++){
        for(int j = 0; j < bi->size; j++){
            if(doubleword++ % 4 == 0) cout << "|";
            if(!bi->use) cout << "-";
            else cout << bi->PID;
        }
    }
    cout << "|" <<endl;
    doubleword = 4;
    //Physical Memory (AID)
    printf("%-30s", ">> Physical Memory (AID): ");
    for(bi = buddysys.begin(); bi != buddysys.end(); bi++){
        for(int j = 0; j < bi->size; j++){
            if(doubleword++ % 4 == 0) cout << "|";
            if(!bi->use) cout << "-";
            else cout << bi->AID;
        }
    }
    cout << "|" <<endl;
    doubleword = 4;
    for(int i = 0; i < process.size(); i++){
        doubleword = 4;
        //page table(AID) by each process
        printf(">> PID(%d) %-20s", i, "Page Table (AID): ");
        for(int j = 0; j < virmem; j++){
            if(doubleword++ % 4 == 0) cout << "|";
            if(printvm[i][0][j] != -1) cout << printvm[i][0][j];
            else cout << "-";
        }
        cout << "|" <<endl;
        doubleword = 4;
        //page table(Valid) by each process
        printf(">> PID(%d) %-20s", i, "Page Table (Valid): ");
        for(int j = 0; j < virmem; j++){
            if(doubleword++ % 4 == 0) cout << "|";
            if(printvm[i][1][j] != -1) cout << printvm[i][1][j];
            else cout << "-";
        }
        cout << "|" <<endl;
        doubleword = 4;
        //page table(FI) by each process
        printf(">> PID(%d) %-20s", i, "Page Table (FI): ");
        for(int j = 0; j < virmem; j++){
            if(doubleword++ % 4 == 0) cout << "|";
            if(printvm[i][2][j] != -1) cout << printvm[i][2][j];
            else cout << "-";
        }
        cout << "|" <<endl;
    }
    cout << endl;
}

//main function
int main(int argc, char* argv[]){
    mode = readmode(argc, argv[1]);
    getInput();

    Buddy init = {-1, -1, false, phymem, 0};
    buddysys.push_back(init);
    runinstr = 0;
    //for each instruction
    for(int i = 0; i < ninstr; i++){
        if(i % 8 == 0) updatebyte();
        runinstr++;
        if(instr[i][0] == 0) memAlloc(instr[i][1], instr[i][2]);
        else if(instr[i][0] == 1) memAccess(instr[i][1], instr[i][2]);
        else if(instr[i][0] == 2) memRelease(instr[i][1], instr[i][2]);
        printResult();
    }
    cout << pageFault << endl;
}
