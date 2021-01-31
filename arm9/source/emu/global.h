#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#ifdef __cplusplus
extern "C" {
#endif

//#include "gp2x_psp.h"
#include <time.h>

# ifndef CLK_TCK
# define CLK_TCK  CLOCKS_PER_SEC
# endif

#define CPU_CLOCK_IDLE 60
#define CPU_CLOCK_STD 200

# define ATARI_RENDER_NORMAL        0
# define ATARI_RENDER_FIT_WIDTH     1
# define ATARI_RENDER_FIT           2
# define ATARI_RENDER_MAX           3

# define MAX_PATH   256
# define ATARI_MAX_SAVE_STATE 5

#ifdef __cplusplus
}
#endif

#endif
