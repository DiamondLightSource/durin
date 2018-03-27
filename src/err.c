/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */


#include <stdio.h>
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
	stack.files[idx] = files_buffer + (idx * ERR_MAX_FILENAME_LENGTH);
	stack.funcs[idx] = funcs_buffer + (idx * ERR_MAX_FUNCNAME_LENGTH);
	stack.messages[idx] = messages_buffer + (idx * ERR_MAX_MESSAGE_LENGTH);

	/* subtract 1 to ensure room for null byte in buffer */
	sprintf(stack.funcs[idx], "%.*s", ERR_MAX_FUNCNAME_LENGTH - 1, func);
	sprintf(stack.files[idx], "%.*s", ERR_MAX_FILENAME_LENGTH - 1, file);
	sprintf(stack.messages[idx], "%.*s", ERR_MAX_MESSAGE_LENGTH - 1, message);
	stack.lines[idx] = line;
	stack.errors[idx] = err;

	stack.size++;
}


void reset_error_stack() {
	stack.size = 0;
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
