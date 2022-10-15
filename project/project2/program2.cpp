#include <time.h>
#include <iostream>
#include <cstring>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <unistd.h>

using std::string;
using std::to_string;


//for 3 mode
const char* max = "max";
const char* avg = "avg";
const char* min = "min";

//info data
int row, col, kernel;
int result_row, result_col;
int maxnumcp;
int MaxProcess;

//Data for information for processes
int **input;
int *numperpro;
int **workindex;

//Shared memory
int *input_data;
int *output_data;
int *information;

//Functions for operations
void getdata();
void divide_work();
void print_result();
int* make_shm_sg(int shm);
int make_shm(key_t key, int size);
void del_shm(int shm);

int main(int argc, char* argv[]) {
    long execution_time;
    timespec begin, end;
    clock_gettime(CLOCK_MONOTONIC, &begin);

    pid_t* pid;
    pid_t cur;

    MaxProcess = strtol(argv[2], NULL, 10);

    getdata();
    divide_work();

    key_t key_in = 6523;
    key_t key_out = 6234;
    key_t key_ifm = 7501;
    
    int in_shm, out_shm, info_shm;

    in_shm = make_shm(key_in, row * col);
    out_shm = make_shm(key_out, result_row * result_col);
    info_shm = make_shm(key_ifm, 3);

    input_data = make_shm_sg(in_shm);
    output_data = make_shm_sg(out_shm);
    information = make_shm_sg(info_shm);

    information[0] = row;
    information[1] = col;
    information[2] = kernel;

    //Copy input data to shared memory
    for(int i = 0; i < row; i++){
        for(int j = 0; j < col; j++){
            input_data[i * col + j] = input[i][j];
        }
    }

    //Create child processes as many as a given number
    pid = new pid_t[MaxProcess];
    int ditb = 0;
    string temp1, temp2, temp3;
    for(int i = 0; i < MaxProcess; i++){
        if(numperpro[i] == 0) break;
        pid[i] = fork();
        cur = pid[i];
        if(pid[i] == 0){
            temp1 = to_string(numperpro[i]);
            temp2 = to_string(workindex[ditb][0]);
            temp3 = to_string(workindex[ditb][1]);
            break;
        }
        ditb += numperpro[i];
    }

    //Arguments to be passed to each process
    const char* worknum = temp1.c_str();
    const char* subrow = temp2.c_str();
    const char* subcol = temp3.c_str();

    if(cur == 0){
        if(strcmp(max, argv[1]) == 0){
            const char* args[] = {"./program1", max, worknum, subrow, subcol, NULL};
            execvp("./program1", (char* const*)args);
        }else if(strcmp(min, argv[1]) == 0){
            const char* args[] = {"./program1", min, worknum, subrow, subcol, NULL};
            execvp("./program1", (char* const*)args);
        }else if(strcmp(avg, argv[1]) == 0){
            const char* args[] = {"./program1", avg, worknum, subrow, subcol, NULL};
            execvp("./program1", (char* const*)args);
        }
    }else{
        int flag;
        for(int i = 0; i < MaxProcess; i++){
            waitpid(pid[i], &flag, 0);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    execution_time = (long)(end.tv_sec - begin.tv_sec)*1000 + ((end.tv_nsec - begin.tv_nsec) / 1000000);
    printf("%ld\n", execution_time);
    print_result();

    //Delete Shared Memory
    del_shm(in_shm);
    del_shm(out_shm);
    del_shm(info_shm);

    return 0;
}

//Generating shared memory and returning memory IDs
int make_shm(key_t key, int size){
    int shm;
    if((shm = shmget(key, sizeof(int) * size, IPC_CREAT|0666))== -1){
        printf("shmget failed\n");
        exit(0);
    }
    return shm;
}

//Returns segments of shared memory
int* make_shm_sg(int shm){
    void *mem_seg = NULL;
    if((mem_seg = shmat(shm, NULL, 0)) == (void*) -1){
        printf("shmat failed\n");
        exit(0);
    }
    return (int *)mem_seg;
}

//Divide the work according to the given number of processes.
void divide_work(){
    numperpro = new int[MaxProcess];
    for(int i = 0; i < MaxProcess; i++){
        numperpro[i] = (int) maxnumcp / MaxProcess;
    }
    for(int i = 0; i < maxnumcp % MaxProcess; i++){
        numperpro[i] += 1;
    }
    
    workindex = new int*[maxnumcp];
    for(int i = 0; i < maxnumcp; i++) workindex[i] = new int[2];

    int x = 0;
    for(int i = 0; i < row; i += kernel){
        for(int j = 0; j < col; j += kernel){
            workindex[x][0] = i;
            workindex[x][1] = j;
            x++;
        }
    }
}

//Receive data and store it in the appropriate variable
void getdata() {
    scanf("%d %d %d", &row, &col, &kernel);
    maxnumcp = int((row / kernel) * (col / kernel));
    result_row = int(row / kernel);
    result_col = int(col / kernel);

    input = new int*[row];
    for (int i = 0; i < row; i++) input[i] = new int[col];

    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            scanf("%d", &input[i][j]);
        }
    }

    return;
}

//Output of results
void print_result() {
    for (int i = 0; i < result_row; i++) {
        for (int j = 0; j < result_col; j++) {
            printf("%d ", output_data[i * result_col + j]);
        }
        printf("\n");
    }
}

//Delete Shared Memory
void del_shm(int shm){
    if(shmctl(shm, IPC_RMID,NULL) == -1){
        printf("shmctl failed\n");
    }
    return;
}