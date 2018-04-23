/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */


#include <stdio.h>
#include <hdf5.h>
#include "err.h"


struct error_stack_t {
	char **files;
	char **funcs;
	int *lines;
	int *errors;
	char **messages;
	int size;
};

static char files_buffer[ERR_MAX_FILENAME_LENGTH * ERR_MAX_STACK_SIZE] = {0};
static char funcs_buffer[ERR_MAX_FUNCNAME_LENGTH * ERR_MAX_STACK_SIZE] = {0};
static char messages_buffer[ERR_MAX_MESSAGE_LENGTH * ERR_MAX_STACK_SIZE] = {0};

static char *files[ERR_MAX_STACK_SIZE] = {0};
static char *funcs[ERR_MAX_STACK_SIZE] = {0};
static int lines[ERR_MAX_STACK_SIZE] = {0};
static int errors[ERR_MAX_STACK_SIZE] = {0};
static char *messages[ERR_MAX_STACK_SIZE] = {0};

static struct error_stack_t stack = {files, funcs, lines, errors, messages, 0};


void push_error_stack(const char *file, const char *func, int line, int err, const char *message) {
	if (stack.size >= ERR_MAX_STACK_SIZE) return; /* unfortunate */
	int idx = stack.size;

	/* subtract 1 to ensure room for null byte in buffer */
	sprintf(stack.funcs[idx], "%.*s", ERR_MAX_FUNCNAME_LENGTH - 1, func);
	sprintf(stack.files[idx], "%.*s", ERR_MAX_FILENAME_LENGTH - 1, file);
	sprintf(stack.messages[idx], "%.*s", ERR_MAX_MESSAGE_LENGTH - 1, message);
	stack.lines[idx] = line;
	stack.errors[idx] = err;

	stack.size++;
}


herr_t h5e_walk_callback(unsigned int n, const struct H5E_error2_t *err, void *client_data) {
	herr_t retval = 0;
	/* only read the message for the innermost stack frame - the rest are just noise */
	if (n == 0) {
		char message[ERR_MAX_MESSAGE_LENGTH] = {0};
		sprintf(message, "%.*s", ERR_MAX_MESSAGE_LENGTH - 1, err->desc);
		push_error_stack(err->file_name, err->func_name, err->line, -1, message);
	} else {
		push_error_stack(err->file_name, err->func_name, err->line, -1, "");
	}
	return retval;
}


int h5e_error_callback(hid_t stack_id, void *client_data) {
	int retval = 0;
	herr_t err = 0;
	err = H5Ewalk2(stack_id, H5E_WALK_UPWARD, &h5e_walk_callback, client_data);
	if (err < 0) {
		ERROR_JUMP(err, done, "Error walking HDF5 Error stack");
	}
done:
	return retval;
}


void reset_error_stack() {
	stack.size = 0;
	H5Eclear2(H5E_DEFAULT); /* almost certainly unnecessary */
}


void dump_error_stack(FILE *out) {
	int idx = stack.size;
	if (idx > 0) fprintf(out, "Durin plugin error:\n");
	while (idx-- > 0) {
		const char *file = stack.files[idx];
		const char *func = stack.funcs[idx];
		const char *message = stack.messages[idx];
		const int line = stack.lines[idx];
		if (message[0] != '\0') {
			fprintf(out, "\t%s - line %d in %s:\n\t\t%s\n", file, line, func, message);
		} else {
			fprintf(out, "\t%s - line %d in %s\n", file, line, func);
		}
	}
}


int init_h5_error_handling() {
	int retval = 0;
	hid_t err = 0;
	if ((err = H5Eset_auto2(H5E_DEFAULT, &h5e_error_callback, NULL)) < 0) {
		ERROR_JUMP(err, done, "Error configuring HDF5 error callback");
	}
done:
	return retval;
}

int init_error_handling() {
	int retval = 0;
	int idx = 0;
	while (idx < ERR_MAX_STACK_SIZE) {
		stack.files[idx] = files_buffer + (idx * ERR_MAX_FILENAME_LENGTH);
		stack.funcs[idx] = funcs_buffer + (idx * ERR_MAX_FUNCNAME_LENGTH);
		stack.messages[idx] = messages_buffer + (idx * ERR_MAX_MESSAGE_LENGTH);
		idx++;
	}
	return retval;
}
