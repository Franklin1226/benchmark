#include "coding_drc.h"
#include "../storage/blockstorage.h"

#include "../simulation/simulation_common.h"
#include "../jerasure/galois.h"
#include "../jerasure/jerasure.h"
#include "../jerasure/reed_sol.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>

extern struct simulation_state* S_STATE;

int findBlock() {
    int disk_total_num, disk_id, free_offset, i;
    disk_total_num = S_STATE->disk_total_num;
    disk_id = -1;
    free_offset = S_STATE->free_offset[0];

    for(i = 0; i < disk_total_num; i++) {
        if(S_STATE->free_size[i] > 0) {
            if(free_offset >= S_STATE->free_offset[i]) {
                disk_id = i;
                free_offset = S_STATE->free_offset[i];
            }
        }
    }
    return disk_id;
}

int findKDisk(int *dm_ids, int k, int disk_id) {
    int disk_per_rack, disk_total_num, cur_rack, cnt;

    int *rack_rec;

    rack_rec = (int *)malloc(sizeof(int) * S_STATE->rack_num);
    memset(rack_rec, 0, sizeof(int) * S_STATE->rack_num);

    disk_total_num = S_STATE->disk_total_num;
    
    disk_per_rack = disk_total_num / S_STATE->rack_num;

    cnt = 0;
    int i;
    //1.find disks in local rack first
    cur_rack = disk_id / disk_per_rack;

    for(i = cur_rack * disk_per_rack; i < cur_rack * disk_per_rack + disk_per_rack; i++) {
        if(S_STATE->disk_status[i] == 0) dm_ids[cnt++] = i;
        if(cnt == k) break;
    }
    if(cnt == k) return 1;

    rack_rec[cur_rack] = 1;

    //2. find disks in remote rack
    while(1) {
        int max_num = -1, rack_no = -1;
        for(i = 0; i < S_STATE->rack_num; i++) {
            if(rack_rec[i] == 0 && S_STATE->health_disk_in_rack[i] > max_num) {
                max_num = S_STATE->health_disk_in_rack[i];
                rack_no = i;
            }
        }
        if(rack_no == -1) return 0;
        for(i = rack_no * disk_per_rack; i < rack_no * disk_per_rack + disk_per_rack; i++) {
            if(S_STATE->disk_status[i] != 1) {
                dm_ids[cnt++] = i;
                if(cnt == k) return 1;
            }
        }
        rack_rec[rack_no] = 1;
    }
    free(rack_rec);
}
void encode(const char* buf, int size) {
    int disk_id, block_no, block_size, write_size, fault_tolerance, data_disk_num, i, j, k;
    int size_request, block_request;
    int data_disk_coeff, offset;
    int field_power = 8;

    

    char *buf_tmp;//stripe block information
    buf_tmp = (char *)calloc(S_STATE->disk_block_size, sizeof(char));
    
    //m
    fault_tolerance = S_STATE->code_m;
    //k
    data_disk_num = S_STATE->code_k;

    block_size = S_STATE->disk_block_size;

    size_request = round_to_block_size(size);
    block_request = size_request / block_size;
    //write block_request blocks in sequence
    for(i = 0; i < block_request; i++) {
        //disk which data will be written to
        disk_id = S_STATE->cur_disk;
        
        assert(S_STATE->free_size[disk_id] > 0);
        block_no = S_STATE->free_offset[disk_id];
        write_size = (i == block_request - 1) ? (size - block_size * (block_request - 1)) : block_size;
        offset = i * block_size;
        writeDisk(disk_id, buf + offset, write_size, block_no * block_size);
        S_STATE->free_offset[disk_id]++;
        S_STATE->free_size[disk_id]--;
        //update partial encode result
        for(j = 0; j < fault_tolerance; j++) {
            data_disk_coeff = S_STATE->generator_matrix[j*data_disk_num+disk_id];
            for(k = 0; k < write_size; k++) {
                buf_tmp[k] = galois_single_multiply((unsigned char)buf[k+offset], data_disk_coeff, field_power);
            }
            for(k = 0; k < write_size; k++) {
                S_STATE->partial_result[j][k] = S_STATE->partial_result[j][k] ^ buf_tmp[k];
            }
        }
        S_STATE->cur_disk++;
        if(S_STATE->cur_disk == S_STATE->code_k) {
            //time to write encode block
            for(j = 0; j < fault_tolerance; j++) {
                disk_id = S_STATE->cur_disk;
                assert(disk_id != 0);
                block_no = S_STATE->free_offset[disk_id];
                write_size = block_size;
                writeDisk(disk_id, S_STATE->partial_result[j], write_size, block_no * block_size);
                S_STATE->free_offset[disk_id]++;
                S_STATE->free_size[disk_id]--;
                memset(S_STATE->partial_result[j], 0, block_size);
                S_STATE->cur_disk++;
            }
            S_STATE->cur_disk = 0;
        }
    }
    free(buf_tmp);
}

/*
* backup:Backup specified file
* @param path:file path
*/
void backup(const char *path) {
    int fd = open(path, O_RDWR);
    if(fd == -1) {
        printf("Open target file failed during backup\n");
        return ;
    }
    int READ_BUF_LEN = S_STATE->disk_total_num * S_STATE->disk_block_size;
    printf("Read buf len: %d\n", READ_BUF_LEN);
    char *read_buf = (char *)calloc(READ_BUF_LEN, 1);
    int len;
    len = read(fd, read_buf, READ_BUF_LEN);
    while(len > 0) {
        encode(read_buf, len);
        len = read(fd, read_buf, READ_BUF_LEN);
    }
    if(S_STATE->cur_disk != 0) {
        int disk_id = S_STATE->code_k;
        
        for(disk_id = S_STATE->code_k; disk_id < S_STATE->disk_total_num; disk_id++) {
            int fd_parity = open(S_STATE->dev_name[disk_id], O_WRONLY);
            long long offset = S_STATE->free_offset[disk_id] * S_STATE->disk_block_size;
            assert(fd_parity != -1);
            pwrite(fd_parity, S_STATE->partial_result[disk_id-S_STATE->code_k], S_STATE->disk_block_size, offset);
            S_STATE->free_offset[disk_id]++;
            S_STATE->free_size[disk_id]--;
            memset(S_STATE->partial_result[disk_id-S_STATE->code_k],0,S_STATE->disk_block_size);
            close(fd_parity);
        }
        S_STATE->cur_disk = 0;
    }
    free(read_buf);
    close(fd);
}
void decode(int disk_id, char* buf, long long size, long long offset, int* dm_ids) {

    struct timeval starttime, endtime;

    char* buf_temp;
    char* temp_buf;
    int i;
    long long j;

    int *decoding_matrix;
        

    int data_disk_num;

    data_disk_num = S_STATE->code_k;

    int block_size;
    int block_no;

    int blockoffset;
    int diskblocklocation;


    block_size = S_STATE->disk_block_size;
    block_no = offset / block_size;
    blockoffset = offset - block_no * block_size;

    temp_buf = (char *)malloc(sizeof(char) * size);
    memset(temp_buf, 0, size);

    buf_temp = (char *)malloc(sizeof(char) * size);
    memset(buf_temp, 0, size);

    memset(buf, 0, size);

    int *intbuf_temp = (int *)buf_temp;
    int *intbuf = (int *)buf;
        


    //find all the failed disks
    

        

    decoding_matrix = (int *)malloc(sizeof(int) * data_disk_num * data_disk_num);

    if(jerasure_make_decoding_matrix(data_disk_num, S_STATE->code_m, 8,
                                        S_STATE->generator_matrix,
                                        decoding_matrix, dm_ids) < 0) {
        //free(dm_ids);
        free(decoding_matrix);
        free(buf_temp);
        free(temp_buf);
        printf("Generate decode matrix failed\n");
        return ;  
    }

    for(i = 0; i < data_disk_num; i++) {
        diskblocklocation = block_no * block_size + blockoffset;

        int efficient = decoding_matrix[data_disk_num * disk_id + i];

        int retstat = readDisk(dm_ids[i], temp_buf, size, diskblocklocation);
        assert(retstat >= 0);
        gettimeofday(&starttime, NULL);
        for(j = 0; j < size; j++) {
            buf_temp[j] = galois_single_multiply((unsigned char)temp_buf[j], efficient, 8);
        }

        for(j = 0; j < (long long)(size * sizeof(char) / sizeof(int)); j++) {
            intbuf[j] = intbuf[j] ^ intbuf_temp[j];
        }
        gettimeofday(&endtime, NULL);

        S_STATE->recover_time_drc = S_STATE->recover_time_drc + 
            endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec - starttime.tv_usec) / 1000000.0;
        
        S_STATE->recover_time_rs = S_STATE->recover_time_rs +
            endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec - starttime.tv_usec) / 1000000.0;
        memset(temp_buf, 0, size);
    }

    //free(dm_ids);
    free(decoding_matrix);
    free(temp_buf);
    free(buf_temp);

    return ;
    
}

void recover(int disk_id, const char* newdevice) {
    checkDiskStatus(S_STATE);
    int disk_per_rack = S_STATE->disk_total_num / S_STATE->rack_num;
    
    if(S_STATE->disk_status[disk_id] == 0) {
        printf("This disk is healthy\n");
        return ;
    }
    int healthy_num = 0, i;
    for(i = 0; i < S_STATE->disk_total_num; i++) {
            
        if(S_STATE->disk_status[i] == 0) {
            healthy_num++;
            S_STATE->health_disk_in_rack[i / disk_per_rack]++;
        }
    }
    if(healthy_num < S_STATE->code_k) {
        printf("Do not have enough healthy disk for encoding\n");
        return ;
    }
    S_STATE->dev_name[disk_id] = (char *)realloc(S_STATE->dev_name[disk_id], strlen(newdevice));
    memset(S_STATE->dev_name[disk_id], 0, strlen(newdevice) + 1);
    strncpy(S_STATE->dev_name[disk_id], newdevice, strlen(newdevice));
    if(access(S_STATE->dev_name[disk_id], 0) != 0) {
        int fd = open(S_STATE->dev_name[disk_id], O_RDWR | O_CREAT, S_IRWXU);
        assert(fd != -1);
        lseek(fd, S_STATE->disk_size- 1, SEEK_SET);
        write(fd, "0", 1);
        lseek(fd, 0, SEEK_SET);
        close(fd);
    }
    
    int _recoversize = S_STATE->disk_size;
    int block_size = S_STATE->disk_block_size;
    _recoversize /= block_size;
    char buf[block_size];

    int* dm_ids = (int *)malloc(sizeof(int) * S_STATE->code_k);
    
    findKDisk(dm_ids, S_STATE->code_k, disk_id);
    

    int disk_in_rack = 0, disk_cross_rack = 0;
    int* rack_rec = (int *)calloc(S_STATE->rack_num, sizeof(int));
    for(i = 0; i < S_STATE->code_k; i++) {
        int cur_disk = dm_ids[i];
        rack_rec[cur_disk / disk_per_rack]++;
    }
    int failed_rack = disk_id / disk_per_rack;
    for(i = 0; i < S_STATE->rack_num; i++) {
        if(rack_rec[i] == 0) continue;
        if(i == failed_rack) {
            disk_in_rack += rack_rec[i];
        } else {
            disk_in_rack += (rack_rec[i] - 1);
            disk_cross_rack++;
        }
    }
    

    
    double time = 0.0;
    time = (double)1.0 * disk_cross_rack *  block_size / (CROSSRACK_BANDWIDTH);
    time = (double)1.0 * time + disk_in_rack * block_size / (INRACK_BANDWIDTH);
    double time_rs = 0;
    time_rs = 1.0 * (S_STATE->code_k - (disk_per_rack - 1)) * block_size / (CROSSRACK_BANDWIDTH);
    for(i = 0; i < _recoversize; i++) {
        memset(buf, 0, block_size);
        int offset = i * block_size;

        decode(disk_id, buf, block_size, offset, dm_ids);

        
        //printf("%lf\n", time);
        S_STATE->recover_time_drc += time;
        S_STATE->recover_time_rs += time_rs;
        S_STATE->transfer_cost_rs += time_rs;
        S_STATE->transfer_cost_drc += time;

        writeDisk(disk_id, buf, block_size, offset);
    }
    free(dm_ids);
    free(rack_rec);
}