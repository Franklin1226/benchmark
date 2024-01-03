#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "simulation_common.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "../coding/coding_drc.h"
#include "../storage/blockstorage.h"
#include "../jerasure/galois.h"
#include "../jerasure/jerasure.h"
#include "../jerasure/reed_sol.h"

struct simulation_state* S_STATE;


int main(int argc, char **argv) {
    struct simulation_state* s_state;
    s_state = (struct simulation_state *)malloc(sizeof(struct simulation_state));
    //1.Initialize disks information
    initSimulationConfig(s_state);

    S_STATE = s_state;

    //2.Check disk status
    checkDiskStatus(s_state);
    //3.Choos the option
    printf("-----------------------------\n");
    printf("|                           |\n");
    printf("|\t  1.Backup\t    |\n");
    printf("|\t  2.Recover\t    |\n");
    printf("|\t  3.Exit\t    |\n");
    printf("|                           |\n");
    printf("-----------------------------\n");
    
    char choice;
    while(1) {
        printf("Please enter your operation:\n");
        scanf("%c", &choice);
        getchar();
        
        switch(choice) {
            case '1':
                //backup function
                printf("Starting backup\n");
                printf("Please enter the backup path:\n");
                char path[PATH_MAX];
                scanf("%s", path);
                getchar();
                backup(path);
                printf("Finish backup\n");
                
                break;
            case '2':
                printf("Starting recover\n");
                printf("********Recover**********\n");
                printf("Please enter newdevice path:\n");
                char newdevice[PATH_MAX];
                scanf("%s", newdevice);
                getchar();
                recover(0, newdevice);
                printf("Finish recover\n");
                printf("DRC recover time is %lf\n", s_state->recover_time_drc);
                printf("Conventional recover time is %lf\n", s_state->recover_time_rs);
                printf("DRC transfer cost is %lf\n", s_state->transfer_cost_drc);
                printf("Conventional transfer cost is %lf\n", s_state->transfer_cost_rs);
                break;
            case '3':
                printf("Exit successfully\n");
                exit(0);
            default:
                printf("illegal input:%d\n", choice);
                break;
        }
    }
    free(s_state);
    return 0;
}