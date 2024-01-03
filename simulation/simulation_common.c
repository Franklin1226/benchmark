#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "simulation_common.h"

#include "../jerasure/galois.h"
#include "../jerasure/jerasure.h"
#include "../jerasure/reed_sol.h"

extern struct simulation_state* S_STATE;

void initSimulationConfig(struct simulation_state* s_state) {
    s_state->code_k = CODE_K;
    s_state->code_m = CODE_M;
    s_state->disk_total_num = DISK_TOTAL_NUM;
    s_state->disk_block_size = DISK_BLOCK_SIZE;
    s_state->disk_size = DISK_SIZE;
    s_state->cur_disk = 0;
    s_state->rack_num = RACK_NUM;
    s_state->recover_time_drc = 0;
    s_state->recover_time_rs = 0;
    s_state->transfer_cost_drc = 0;
    s_state->transfer_cost_rs = 0;
    //s_state->disk_fd = (int *)malloc(sizeof(int) * s_state->disk_total_num);
    s_state->health_disk_in_rack = (int *)malloc(sizeof(int) * s_state->rack_num);
    memset(s_state->health_disk_in_rack, 0, sizeof(int) * s_state->rack_num);
    s_state->disk_status = (int *)malloc(sizeof(int) * s_state->disk_total_num);
    s_state->free_offset = (int *)malloc(sizeof(int) * s_state->disk_total_num);
    s_state->free_size = (int *)malloc(sizeof(int) * s_state->disk_total_num);
    s_state->dev_name = (char **)calloc(s_state->disk_total_num, sizeof(char *));
    s_state->partial_result = (char **)calloc(s_state->code_m, sizeof(char *));
    s_state->generator_matrix = reed_sol_vandermonde_coding_matrix(s_state->code_k, s_state->code_m, 8);
    int i, fd;

    for(i = 0; i < s_state->code_m; i++) {
        s_state->partial_result[i] = (char *)calloc(s_state->disk_block_size, sizeof(char));
    }

    char tempString[256];
    for(i = 0; i < s_state->disk_total_num; i++) {
        memset(tempString, 0, 256);
        sprintf(tempString, "disk%d", i);
        s_state->dev_name[i] = (char *)calloc(PATH_MAX, 1);
        strncpy(s_state->dev_name[i], tempString, PATH_MAX);
        s_state->free_offset[i] = 0;
        s_state->free_size[i] = s_state->disk_size / s_state->disk_block_size;
        
        fd = open(s_state->dev_name[i], O_RDWR | O_CREAT, S_IRWXU);
        if(fd == -1) {
            printf("create disk file%d failed\n", i);
            exit(1);
        }
        lseek(fd, s_state->disk_size- 1, SEEK_SET);
        write(fd, "\0", 1);
        lseek(fd, 0, SEEK_SET);
        close(fd);

        
    }
}

void checkDiskStatus(struct simulation_state *s_state) {
    int i, fd;
    for(i = 0; i < s_state->disk_total_num; i++) {
        fd = open(s_state->dev_name[i], O_RDWR);
        if(fd != -1) {
            s_state->disk_status[i] = 0;
            printf("***get disk status: open good: i=%d\n", i);
            close(fd);
        } else {
            s_state->disk_status[i] = 1;
            printf("***get disk status: open bad: i=%d\n", i);
            //printf("***dev name is %s\n", s_state->dev_name[i]);
        }
    }
}

void flush() {
    char ch;
    while((ch = getchar()) != EOF);
}

int round_to_block_size(int size) {
    int result;
    int disk_block_size;

    disk_block_size = S_STATE->disk_block_size;

    if((size % disk_block_size) != 0) {
        result = size + disk_block_size - (size % disk_block_size);
    } else {
        result = size;
    }
    return result;
}

int cmpZero(char* buf, int size) {
    int i;
    for(i = 0; i < size; i++) {
        if(buf[i] != 0) return 0;
    }
    return 1;
}