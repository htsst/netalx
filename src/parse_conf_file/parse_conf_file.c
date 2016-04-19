/* ------------------------------------------------------------------------ *
 * This is part of NETALX.
 *
 * Copyright (C) 2013-2015 The GraphCREST Project, Tokyo Institute of Technology
 *
 * NETALX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * ------------------------------------------------------------------------ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif

#define _FILE_OFFSET_BITS 64


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_conf_file.h"


char *original_strdup(const char *s)
{
    char *t;
    t = (char *)malloc(strlen(s) + 1);
    if (t == NULL)
        return (NULL);

    strcpy(t, s);
    return (t);
}

struct config_list_t make_configlist_from_conffile(const char *fname, const char **key_list, size_t num_keys)
{

	struct config_list_t config_list;

	config_list.list_lenght = num_keys;

	char **value_list  = (char **)malloc(sizeof(char *) * num_keys);
	config_list.value_list  = value_list;



	FILE *fp = fopen(fname, "r");
	char buf[MAX_CONFIG_LINE_LENGTH+1];

	while ( fgets(buf, MAX_CONFIG_LINE_LENGTH, fp) != NULL ) {

		char *key = strtok(buf, DELIM);
		if (key == NULL) continue;

		// ignore top whitespaces
		int top_whilespaces_length = strspn(buf, WHITESPACE);
		key = &key[top_whilespaces_length];


		char *value = strtok(NULL, DELIM);
		if (value == NULL) continue;


		size_t pos_key_end;
		pos_key_end = strcspn(key, DELIM);
		key[pos_key_end] = '\0';
		pos_key_end = strcspn(key, WHITESPACE);
		key[pos_key_end] = '\0';

		size_t pos_value_end = strcspn(value, WHITESPACE);
		value[pos_value_end] = '\0';


		for (size_t k = 0; k < num_keys; k++) {
			if (!strcmp(key, key_list[k])) {
				#if _SVID_SOURCE         || \
					_BSD_SOURCE          || \
					_XOPEN_SOURCE >= 500 || \
					_XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED || \
					_POSIX_C_SOURCE >= 200809L /* Since glibc 2.12: */

					value_list[k] = strdup(value);

				#else
					#warning use original strdup
					value_list[k] = original_strdup(value);

				#endif

			}
		}


	}

#if 0
	printf("result of config file\n");
	for (size_t k = 0; k < config_list.list_lenght; k++) {
		printf("(%s, %s)\n", key_list[k], config_list.value_list[k]);
	}
#endif

	return config_list;

}


void free_config_list(struct config_list_t config_list)
{
	for (size_t k = 0; k < config_list.list_lenght; k++) {
		free(config_list.value_list[k]);
	}
	free(config_list.value_list);

}

