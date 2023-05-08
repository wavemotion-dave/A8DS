/*
 * util.c - utility functions
 *
 * Copyright (c) 2005 Piotr Fusik
 * Copyright (c) 2005 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "atari.h"
#include "util.h"


char *Util_stpcpy(char *dest, const char *src)
{
	size_t len = strlen(src);
	memcpy(dest, src, len + 1);
	return dest + len;
}

char *Util_strncpy(char *dest, const char *src, size_t size) {
	while (size-- > 0) {
		if ((*dest++ = *src++) == '\0')
			break;
	}
	while (size-- > 0)
		*dest++ = '\0';
	return dest;
}

char *Util_strlcpy(char *dest, const char *src, size_t size)
{
	strncpy(dest, src, size);
	dest[size - 1] = '\0';
	return dest;
}

char *Util_strupper(char *s)
{
	char *p;
	for (p = s; *p != '\0'; p++)
		if (*p >= 'a' && *p <= 'z')
			*p += 'A' - 'a';
	return s;
}

char *Util_strlower(char *s)
{
	char *p;
	for (p = s; *p != '\0'; p++)
		if (*p >= 'A' && *p <= 'Z')
			*p += 'a' - 'A';
	return s;
}

void Util_chomp(char *s)
{
	int len;

	len = strlen(s);
	if (len >= 2 && s[len - 1] == '\n' && s[len - 2] == '\r')
		s[len - 2] = '\0';
	else if (len >= 1 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
		s[len - 1] = '\0';
}


void *Util_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL) {
		Atari800_Exit(FALSE);
		printf("Fatal error: out of memory\n");
		exit(1);
	}
    memset(ptr, 0, size);
	return ptr;
}


char *Util_strdup(const char *s)
{
	/* don't use strdup(): it is unavailable on WinCE */
	size_t size = strlen(s) + 1;
	char *ptr = (char *) Util_malloc(size);
	memcpy(ptr, s, size); /* faster than strcpy(ptr, s) */
	return ptr;
}


void Util_catpath(char *result, const char *path1, const char *path2)
{
#ifdef HAVE_SNPRINTF
	snprintf(result, FILENAME_MAX,
#else
	sprintf(result,
#endif
		path1[0] == '\0' || path2[0] == DIR_SEP_CHAR || path1[strlen(path1) - 1] == DIR_SEP_CHAR
#ifdef BACK_SLASH
		 || path2[0] == '/' || path1[strlen(path1) - 1] == '/'
#endif
			? "%s%s" : "%s" DIR_SEP_STR "%s", path1, path2);
}

int Util_fileexists(const char *filename)
{
	FILE *fp;
	fp = fopen(filename, "rb");
	if (fp == NULL)
		return FALSE;
	fclose(fp);
	return TRUE;
}

#if defined(HAVE_STAT)

int Util_direxists(const char *filename)
{
	struct stat filestatus;
	return stat(filename, &filestatus) == 0 && (filestatus.st_mode & S_IFDIR);
}

#else

int Util_direxists(const char *filename)
{
	return TRUE;
}

#endif /* defined(HAVE_STAT) */


int Util_flen(FILE *fp)
{
	fseek(fp, 0, SEEK_END);
	return (int) ftell(fp);
}



