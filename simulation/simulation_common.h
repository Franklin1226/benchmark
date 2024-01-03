#define CODE_K 5
#define CODE_M 4
#define DISK_TOTAL_NUM 9
#define RACK_NUM 9
#define DISK_BLOCK_SIZE 4096
#define DISK_SIZE 1073741824    //1GB

#define PATH_MAX 256
#define FILE_SIZE 100
#define INRACK_BANDWIDTH 1073741824
#define CROSSRACK_BANDWIDTH 107374182
/*
Parameters to be measured:
RS(6,4,3)55.96 RS(6,4,6)67.71 RS(8,6,4)84.55 RS(8,6,8)99.71 DRC(6,4,3)45.96 DRC(8,6,4)64.55 | N-K = 2
RS(6,3,3)47.85 RS(6,3,6)53.59 RS(9,6,3)74.79 RS(9,6,9)98.72 DRC(6,3,3)37.85 DRC(9,6,3)54.79 | N-K = 3
RS(9,5,3)65.02 RS(9,5,9)80.80 DRC(9,5,3)45.02
*/
struct simulation_state {
    int code_k;
    int code_m;
    int disk_total_num;
    int disk_block_size;
    long long disk_size;
    int rack_num;
    
    int* free_offset;
    int* free_size;
    int* disk_status;
    char** dev_name;
    int* health_disk_in_rack;

    char** partial_result;
    int *generator_matrix;
    int cur_disk;

    double recover_time_drc;
    double recover_time_rs;
    double transfer_cost_drc;
    double transfer_cost_rs;
};

void initSimulationConfig(struct simulation_state *s_state);
void checkDiskStatus(struct simulation_state *s_state);
void flush();
int round_to_block_size(int size);
int cmpZero(char* buf, int size);