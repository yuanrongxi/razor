/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <windows.h>
#include <time.h>
#include "audio_log.h"
#include "gettimeofday.h"
#include <direct.h>
#include <string>

using namespace std;

#define PATH_MAX_SIZE 1024

#pragma warning(disable: 4996)

typedef struct log_file_s
{
	FILE*							fp;
	char							filename[PATH_MAX_SIZE];
	CRITICAL_SECTION				mutex;
}log_file_t;

static log_file_t* log_file = NULL;

#define	WT_SPIN_COUNT 4000

const char* get_time_str(char *date_str)
{
	struct tm tm_now;
	struct timeval tv;
	gettimeofday(&tv, NULL);

	time_t now = tv.tv_sec;
	::localtime_s(&tm_now, &now);


	sprintf(date_str, "%04d-%02d-%02d %02d:%02d:%02d.%3ld", tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
		tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, tv.tv_usec / 1000);

	return date_str;
}

static void get_fullexepath(string path)
{
	char buffer[MAX_PATH];
	char* ptr = _getcwd(buffer, MAX_PATH);
	path = ptr;
	path += "\\";
}

int open_win_log(const char* filename)
{
	if (log_file != NULL || filename == NULL)
		return -1;

	log_file = (log_file_t *)calloc(1, sizeof(log_file_t));

	string path;
	get_fullexepath(path);

	path += filename;

	strcpy(log_file->filename, path.c_str());
	log_file->fp = fopen(log_file->filename, "w");
	if (log_file->fp == NULL){
		printf("open %s failed!\r\n", log_file->filename);
		return -1;
	}

	/*³õÊ¼»¯spin mutexËø*/
	InitializeCriticalSectionAndSpinCount(&(log_file->mutex), WT_SPIN_COUNT);
	return 0;
}

void close_win_log()
{
	if (log_file != NULL && log_file->fp != NULL){
		fflush(log_file->fp);
		fclose(log_file->fp);
		log_file->fp = NULL;

		DeleteCriticalSection(&(log_file->mutex));

		free(log_file);
		log_file = NULL;
	}
}

static const char* get_file_name(const char* pathname)
{
	if (pathname == NULL)
		return pathname;

	int32_t size = strlen(pathname);

	char *pos = (char *)pathname + size;
	while (*pos != '\\' && pos != pathname)
		pos--;

	if (pos == pathname)
		return pathname;
	else
		return pos + 1;
}

#define DATE_STR_SIZE 64
int log_win_write(int level, const char* file, int line, const char* fmt, va_list vl)
{
	char date_str[DATE_STR_SIZE];
	if (log_file != NULL && log_file->fp != NULL){
		EnterCriticalSection(&(log_file->mutex));
		fprintf(log_file->fp, "%s %s:%d ", get_time_str(date_str), get_file_name(file), line);
		vfprintf(log_file->fp, fmt, vl);
		LeaveCriticalSection(&(log_file->mutex));
		fflush(log_file->fp);
	}

	return 0;
}



