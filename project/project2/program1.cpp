#include <time.h>
#include <iostream>
#include <cstring>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <unistd.h>


//for 3 modes
const char *max = "max";
const char *avg = "avg";
const char *min = "min";

//info data
int mode; // 0: min, 1: avg, 2: max
int row, col, kernel;
int result_row, result_col;
int **matrix;

//for shared memory
int *input_data;
int *output_data;
int *information;

//for common implementation
void getdata();
void print_result();

//for it's called by another program
int* make_shm(key_t key);
void mat_del(int* shm);
int compute_max(int l, int k, int s);
int compute_min(int l, int k, int s);
int compute_avg(int l, int k, int s);
void result_(int mode, int i, int j, int work);

//common information
int compute_max(int l, int k);
int compute_min(int l, int k);
int compute_avg(int l, int k);
void result_(int mode);


int main(int argc, char* argv[]) {
    long execution_time;
    timespec begin, end;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    //for matrix field
    

    if(strcmp(max, argv[1]) == 0) mode = 2;
    if(strcmp(avg, argv[1]) == 0) mode = 1;
    if(strcmp(min, argv[1]) == 0) mode = 0;

    //common implementation
    if(argc <= 2){

        getdata();
        result_(mode);

        clock_gettime(CLOCK_MONOTONIC, &end);
        execution_time = (long)(end.tv_sec - begin.tv_sec)*1000 + ((end.tv_nsec - begin.tv_nsec) / 1000000);
        printf("%ld\n", execution_time);
        print_result();

        return 0;
    }else{
        //it's called by another program
        //declaring and using shared memory
        key_t key_in = 6523;
        key_t key_out = 6234;
        key_t key_info = 7501;
        
        int worknum, subrow, subcol; // 2, 3, 4

        information = make_shm(key_info);

        row = information[0];
        col = information[1];
        kernel = information[2];

        result_row = row / kernel;
        result_col = col / kernel;
        
        input_data = make_shm(key_in);
        output_data = make_shm(key_out);
        
        worknum = strtol(argv[2], NULL, 10);
        subrow = strtol(argv[3], NULL, 10);
        subcol = strtol(argv[4], NULL, 10);

        result_row = int(row / kernel);
        result_col = int(col / kernel);
        
        subrow /= kernel;
        subcol /= kernel;

        //calculating and storing result values
        result_(mode, subrow, subcol, worknum);

        //remove association with shared memory
        mat_del(input_data);
        mat_del(output_data);
        mat_del(information);
    }
    return 0;
}

//get data from user input
void getdata() {
    scanf("%d %d %d", &row, &col, &kernel);
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

//Calculation of max in the l,kth field
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

//Calculation of min in the l,kth field
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

//Calculation of avg in the l,kth field
int compute_avg(int l, int k) {
    l *= kernel;
    k *= kernel;
    int avgv = 0;
    for (int i = l; i < kernel + l; i++) {
        for (int j = k; j < kernel + k; j++) avgv += matrix[i][j];
    }
    return int(avgv / (kernel * kernel));
}

//Calculation of max in the l,kth field
int compute_max(int l, int k, int s) {
    l *= kernel;
    k *= kernel;
    int maxv = input_data[l * col + k];
    for (int i = l; i < kernel + l; i++) {
        for (int j = k; j < kernel + k; j++) {
            if (maxv < input_data[i * col + j]) maxv = input_data[i * col + j];
        }
    }
    return maxv;
}

//Calculation of min in the l,kth field
int compute_min(int l, int k, int s) {
    l *= kernel;
    k *= kernel;
    int minv = input_data[l * col + k];
    for (int i = l; i < kernel + l; i++) {
        for (int j = k; j < kernel + k; j++) {
            if (minv > input_data[i * col + j]) minv = input_data[i * col + j];
        }
    }
    return minv;
}

//Calculation of avg in the l,kth field
int compute_avg(int l, int k, int s) {
    l *= kernel;
    k *= kernel;
    int avgv = 0;
    for (int i = l; i < kernel + l; i++) {
        for (int j = k; j < kernel + k; j++) avgv += input_data[i * col + j];
    }
    return int(avgv / (kernel * kernel));
}

//store result 
void result_(int mode) {
    if (mode == 0) {
        for (int i = 0; i < result_row; i++) {
            for (int j = 0; j < result_col; j++) matrix[i][j] = compute_min(i, j);
        }
    }
    else if (mode == 1) {
        for (int i = 0; i < result_row; i++) {
            for (int j = 0; j < result_col; j++) matrix[i][j] = compute_avg(i, j);
        }
    }
    else if (mode == 2) {
        for (int i = 0; i < result_row; i++) {
            for (int j = 0; j < result_col; j++) matrix[i][j] = compute_max(i, j);
        }
    }
    return;
}

//store result
void result_(int mode, int i, int j, int work) {
    if (mode == 0) {
        for (; i < result_row; i++) {
            for (; j < result_col; j++) {
                output_data[i * result_col + j] = compute_min(i, j, 0);
                work--;
                if(work == 0) break;
            }
            j = 0;
            if(work == 0) break;
        }
    }
    else if (mode == 1) {
        for (; i < result_row; i++) {
            for (; j < result_col; j++) {
                output_data[i * result_col + j] = compute_avg(i, j, 0);
                work--;
                if(work == 0) break;
            }
            j = 0;
            if(work == 0) break;
        }
    }
    else if (mode == 2) {
        for (; i < result_row; i++) {
            for (; j < result_col; j++) {
                output_data[i * result_col + j] = compute_max(i, j, 0);
                work--;
                if(work == 0) break;
            }
            if(work == 0) break;
            j = 0;
        }
    }

    return;
}

//print result
void print_result() {
    for (int i = 0; i < result_row; i++) {
        for (int j = 0; j < result_col; j++) {
            printf("%d ", matrix[i][j]);
        }
        printf("\n");
    }
}

//Creating shared memory and controlling errors
int* make_shm(key_t key){
    void *mem_seg = NULL;
    int shm;
    if((shm = shmget(key, 0, IPC_CREAT|0666))== -1){
        printf("shmget failed\n");
        exit(0);
    }
    if((mem_seg = shmat(shm, NULL, 0)) == (void*) -1){
        printf("shmat failed\n");
        exit(0);
    }
    return (int *)mem_seg;
}
//Unlink shared memory
void mat_del(int* shm){
    if(shmdt(shm) == -1) printf("shmdt failed\n");
    return;
}