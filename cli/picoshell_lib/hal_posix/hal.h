/*! @file
  @brief
  Hardware abstraction layer
        for POSIX

  <pre>
  Copyright (C) 2016-2020 Kyushu Institute of Technology.
  Copyright (C) 2016-2020 Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.
  </pre>
*/

#ifndef MRBC_SRC_HAL_H_
#define MRBC_SRC_HAL_H_

#ifdef __cplusplus
extern "C" {
#endif

/***** Feature test switches ************************************************/
/***** System headers *******************************************************/
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>

/***** Local headers ********************************************************/
/***** Constant values ******************************************************/
/***** Macros ***************************************************************/
#ifndef MRBC_SCHEDULER_EXIT
#define MRBC_SCHEDULER_EXIT 1
#endif

#if !defined(MRBC_TICK_UNIT)
#define MRBC_TICK_UNIT_1_MS   1
#define MRBC_TICK_UNIT_2_MS   2
#define MRBC_TICK_UNIT_4_MS   4
#define MRBC_TICK_UNIT_10_MS 10
// Congiguring small value for MRBC_TICK_UNIT may cause a decline of timer
// accracy depending on kernel constant HZ and USER_HZ.
// For more information about it on `man 7 time`.
#define MRBC_TICK_UNIT MRBC_TICK_UNIT_4_MS
// Substantial timeslice value (millisecond) will be
// MRBC_TICK_UNIT * MRBC_TIMESLICE_TICK_COUNT (+ Jitter).
// MRBC_TIMESLICE_TICK_COUNT must be natural number
// (recommended value is from 1 to 10).
#define MRBC_TIMESLICE_TICK_COUNT 3
#endif


/***** Typedefs *************************************************************/
/***** Global variables *****************************************************/

extern int hal_fd;

/***** Function prototypes **************************************************/
void mrbc_tick(void);

#ifndef MRBC_NO_TIMER
void hal_init(void);
void hal_enable_irq(void);
void hal_disable_irq(void);
# define hal_idle_cpu()    sleep(1) // maybe interrupt by SIGINT

#else // MRBC_NO_TIMER
# define hal_init()        ((void)0)
# define hal_enable_irq()  ((void)0)
# define hal_disable_irq() ((void)0)
# define hal_idle_cpu()    (usleep(MRBC_TICK_UNIT * 1000), mrbc_tick())

#endif


/***** Inline functions *****************************************************/

//================================================================
/*!@brief
  Open

  @param  pathname
  @param  flags
*/
inline static int hal_open(const char *pathname, int flags)
{
  return (int)open(pathname, flags);
}

//================================================================
/*!@brief
  FD_SET

  @param  fd    dummy
  @param
*/
inline static void hal_FD_SET(int fd, fd_set *readfds)
{
  FD_SET(hal_fd, readfds);
}

//================================================================
/*!@brief
  select

  @param  nfds    dummy
  @param
*/
inline static int hal_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  return (int)select(hal_fd + 1, readfds, writefds, exceptfds, timeout);
}


//================================================================
/*!@brief
  Read

  @param  fd    file descriptor.
  @param  buf   pointer of buffer.
  @param  nbytes        output byte length.
*/
inline static int hal_read(int fd, void *buf, int nbytes)
{
  return (int)read(hal_fd, buf, nbytes);
}


//================================================================
/*!@brief
  Write

  @param  fd    file descriptor.
  @param  buf   pointer of buffer.
  @param  nbytes        output byte length.
*/
inline static int hal_write(int fd, const void *buf, int nbytes)
{
  /* FIXME
   * I should fix
   *   alloc.c console.c console.h
   * */
  return (int)write(hal_fd, buf, nbytes);
  //return (int)write(fd, buf, nbytes);
}


//================================================================
/*!@brief
  Flush write baffer

  @param  fd    file descriptor.
*/
inline static int hal_flush(int fd)
{
  return fsync(fd);
}


#ifdef __cplusplus
}
#endif

void hal_set_fd(int fd);

#endif // ifndef MRBC_SRC_HAL_H_
