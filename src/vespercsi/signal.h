#ifndef VESPERCSI_SIGNAL_H
#define VESPERCSI_SIGNAL_H

#include <vespercsi/vespercsi.h>
#include <sys/signal.h>

void vespercsi_signal_init(void);

void vespercsi_signal_sigint_handler(int s);

#endif
