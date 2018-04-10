/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */


#ifndef NXS_XDS_ERR_H
#define NXS_XDS_ERR_H


#define ERR_MAX_FILENAME_LENGTH 64
#define ERR_MAX_FUNCNAME_LENGTH 128
#define ERR_MAX_MESSAGE_LENGTH 1024
#define ERR_MAX_STACK_SIZE 128


/* obtain __func__ from GCC if no C99 */
#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define __func__ __FUNCTION__
# else
#  define __func__ "<unknown>"
# endif
#endif

#if __GNUC__ >= 2
# define __line__ __LINE__
#else
# define __line__ 0
#endif

#if __GNUC__ >= 2
# define __file__ __FILE__
#else
# define __file__ "unknown"
#endif

#define ERROR_JUMP(err, target, message) \
{ \
	push_error_stack(__file__, __func__, __line__, err, message); \
	retval = err; \
	goto target; \
}

void push_error_stack(const char *file, const char *func, int line, int err, const char *message);

void dump_error_stack(FILE *out);

void reset_error_stack();

int init_h5_error_handling();

#endif /* NXS_XDS_ERR_H */
