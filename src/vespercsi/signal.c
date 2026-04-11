#include <vespercsi/vespercsi.h>
#include <vespercsi/csi.h>
#include <vespercsi/signal.h>
#include <vespercsi/log.h>
#include <vespercsi/plugin.h>

int vespercsi_signal_shutdown_pipe[2] = {0};

volatile sig_atomic_t vespercsi_signal_shutdown_flag = false;

static int vespercsi_signal_sigint_count = 0;

void vespercsi_signal_init(void){

    struct sigaction sa = {
        .sa_handler = vespercsi_signal_sigint_handler
    };

    if(sigaction(SIGINT, &sa, NULL) != 0){
        exit(EXIT_FAILURE);
    }

    if(pipe(vespercsi_signal_shutdown_pipe) != 0){
        exit(EXIT_FAILURE);
    }

    return;
}


void vespercsi_signal_destroy(void){
    close(vespercsi_signal_shutdown_pipe[0]);
    close(vespercsi_signal_shutdown_pipe[1]);
}

void vespercsi_signal_sigint_handler(int s){

    if (vespercsi_signal_shutdown_flag == false) {
        vespercsi_signal_shutdown_flag = true;
        write(vespercsi_signal_shutdown_pipe[1], "\x00", 1);
    }

    vespercsi_signal_sigint_count++;

    if(vespercsi_signal_sigint_count == 3){
        exit(EXIT_SUCCESS);
    }    
}
