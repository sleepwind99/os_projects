#include <time.h>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

//for 3 mode
const char* max = "max";
const char* avg = "avg";
const char* min = "min";

//info data
int mode;
int row, col, kernel;
int result_row, result_col;
int maxnumcp;
int MaxThread;

//Data for information for threads
int** prmt;
int *numperpro;
int **workindex;

//Matrix space containing data
int **matrix;

//Functions for basic operations
void getdata();
void divide_work();
void print_result();
int compute_max(int l, int k);
int compute_min(int l, int k);
int compute_avg(int l, int k);

//Functions for thread execution
void* calculate_result(void* paramt);

int main(int argc, char* argv[]) {
    long execution_time;
    timespec begin, end;
    clock_gettime(CLOCK_MONOTONIC, &begin);

    MaxThread = strtol(argv[2], NULL, 10);

    getdata();
    divide_work();
    
    if(strcmp(max, argv[1]) == 0) mode = 2;
    if(strcmp(avg, argv[1]) == 0) mode = 1;
    if(strcmp(min, argv[1]) == 0) mode = 0;

    //Create parameters to pass on to each thread
    prmt = new int*[MaxThread];
    int ditb = 0;
    for(int i = 0; i < MaxThread; i++){
        prmt[i] = new int[4];
        prmt[i][0] = mode;
        prmt[i][1] = numperpro[i];
        prmt[i][2] = (workindex[ditb][0] / kernel);
        prmt[i][3] = (workindex[ditb][1] / kernel);
        ditb += numperpro[i];
    }

    //Create a pointer to hold a given number of threads
    pthread_t *trd = new pthread_t[MaxThread];
    
    int thread = 0;
    for(int i = 0; i < MaxThread; i++){
        if(prmt[i][1] != 0){
            //Creating a Thread
            pthread_create(&trd[i], NULL, &calculate_result, (void*)prmt[i]);
            thread++;
        }
    }
    
    //Wait for all threads to end
    for(int i = 0; i < thread; i++){
        void* temp;
        pthread_join(trd[i], &temp);
    }
    

    clock_gettime(CLOCK_MONOTONIC, &end);
    execution_time = (long)(end.tv_sec - begin.tv_sec)*1000 + ((end.tv_nsec - begin.tv_nsec) / 1000000);
    printf("%ld\n", execution_time);
    print_result();
    
    return 0;
}

//Calculation of max in the kth field
int compute_max(int l, int k) {
    l *= kernel;
    k *= kernel;
    int maxv = matrix[l][k];
    for (int i = l; i < kernel + l; i++) {
        for (int j = k; j < kernel + k; j++) {
            if (maxv < matrix[i][j]) maxv = matrix[i][j];
        }
    }
    return maxv;
}
//Calculation of min in the kth field
int compute_min(int l, int k) {
    l *= kernel;
    k *= kernel;
    int minv = matrix[l][k];
    for (int i = l; i < kernel + l; i++) {
        for (int j = k; j < kernel + k; j++) {
            if (minv > matrix[i][j]) minv = matrix[i][j];
        }
    }
    return minv;
}
//Calculation of avg in the kth field
int compute_avg(int l, int k) {
    l *= kernel;
    k *= kernel;
    int avgv = 0;
    for (int i = l; i < kernel + l; i++) {
        for (int j = k; j < kernel + k; j++) avgv += matrix[i][j];
    }
    return int(avgv / (kernel * kernel));
}

//Divide the work according to the given number of threads.
void divide_work(){
    numperpro = new int [MaxThread];
    for(int i = 0; i < MaxThread; i++){
        numperpro[i] = (int) maxnumcp / MaxThread;
    }
    for(int i = 0; i < maxnumcp % MaxThread; i++){
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

    matrix = new int*[row];
    for (int i = 0; i < row; i++) matrix[i] = new int[col];

    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            scanf("%d", &matrix[i][j]);
        }
    }

    return;
}

//output of result
void print_result() {
    for (int i = 0; i < result_row; i++) {
        for (int j = 0; j < result_col; j++) {
            printf("%d ", matrix[i][j]);
        }
        printf("\n");
    }
}

//A set of codes that a thread works with
void* calculate_result(void* paramt){
    int* temp = (int*)paramt;
    int i = temp[2];
    int j = temp[3];
    int work = temp[1];
    if (temp[0] == 0) {
        for (; i < result_row; i++) {
            for (; j < result_col; j++) {
                matrix[i][j] = compute_min(i, j);
                work--;
                if(work == 0) break;
            }
            j = 0;
            if(work == 0) break;
        }
    }
    else if (temp[0] == 1) {
        for (; i < result_row; i++) {
            for (; j < result_col; j++) {
                matrix[i][j] = compute_avg(i, j);
                work--;
                if(work == 0) break;
            }
            j = 0;
            if(work == 0) break;
        }
    }
    else if (temp[0] == 2) {
        for (; i < result_row; i++) {
            for (; j < result_col; j++) {
                matrix[i][j] = compute_max(i, j);
                work--;
                if(work == 0) break;
            }
            if(work == 0) break;
            j = 0;
        }
    }
    pthread_exit(NULL);
}