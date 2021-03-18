/*! @file
  @brief
  Hardware abstraction layer
        for POSIX

  <pre>
  Copyright (C) 2016 Kyushu Institute of Technology.
  Copyright (C) 2016 Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.
  </pre>
*/

/***** Feature test switches ************************************************/
/***** System headers *******************************************************/
#include <signal.h>
#include <sys/time.h>


/***** Local headers ********************************************************/
#include "hal.h"


/***** Constat values *******************************************************/
/***** Macros ***************************************************************/
/***** Typedefs *************************************************************/
/***** Function prototypes **************************************************/
/***** Local variables ******************************************************/
#ifndef MRBC_NO_TIMER
static sigset_t sigset_, sigset2_;
#endif


/***** Global variables *****************************************************/
int hal_fd = 1; /* default file descriptor */


/***** Signal catching functions ********************************************/
#ifndef MRBC_NO_TIMER
//================================================================
/*!@brief
  alarm signal handler

*/
static void sig_alarm(int dummy)
{
  mrbc_tick();
}


#endif

/***** Local functions ******************************************************/
/***** Global functions *****************************************************/
#ifndef MRBC_NO_TIMER

//================================================================
/*!@brief
  initialize

*/
void hal_init(void)
{
  sigemptyset(&sigset_);
  sigaddset(&sigset_, SIGALRM);

  // タイマー用シグナル準備
  struct sigaction sa;
  sa.sa_handler = sig_alarm;
  sa.sa_flags   = SA_RESTART;
  sa.sa_mask    = sigset_;
  sigaction(SIGALRM, &sa, 0);

  // タイマー設定
  struct itimerval tval;
  int sec  = 0;
  int usec = MRBC_TICK_UNIT * 1000;
  tval.it_interval.tv_sec  = sec;
  tval.it_interval.tv_usec = usec;
  tval.it_value.tv_sec     = sec;
  tval.it_value.tv_usec    = usec;
  setitimer(ITIMER_REAL, &tval, 0);
}


//================================================================
/*!@brief
  enable interrupt

*/
void hal_enable_irq(void)
{
  sigprocmask(SIG_SETMASK, &sigset2_, 0);
}


//================================================================
/*!@brief
  disable interrupt

*/
void hal_disable_irq(void)
{
  sigprocmask(SIG_BLOCK, &sigset_, &sigset2_);
}


#endif /* ifndef MRBC_NO_TIMER */

void hal_set_fd(int fd)
{
  hal_fd = fd;
}

