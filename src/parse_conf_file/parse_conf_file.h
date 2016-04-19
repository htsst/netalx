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

#ifndef PARSE_CONF_FILE_H
#define PARSE_CONF_FILE_H

#include <stdio.h>


#define MAX_CONFIG_LINE_LENGTH 1024
#define DELIM " \t"
#define WHITESPACE " \t\n"

struct config_list_t {
	size_t list_lenght;
	char **value_list;
};

struct config_list_t make_configlist_from_conffile(const char *fname, const char **key_list, size_t num_keys);
void free_config_list(struct config_list_t config_list);

#endif
