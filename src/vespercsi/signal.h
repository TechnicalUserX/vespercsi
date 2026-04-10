#ifndef VESPERCSI_SIGNAL_H
#define VESPERCSI_SIGNAL_H

#include <vespercsi/vespercsi.h>
#include <sys/signal.h>
#include <stdbool.h>

extern int vespercsi_signal_shutdown_pipe[2];

extern volatile sig_atomic_t vespercsi_signal_shutdown_flag;

void vespercsi_signal_init(void);

void vespercsi_signal_destroy(void);

void vespercsi_signal_sigint_handler(int s);

#endif
