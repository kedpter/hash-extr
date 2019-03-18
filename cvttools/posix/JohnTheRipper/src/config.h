/*
 * This file is part of John the Ripper password cracker,
 * Copyright (c) 1996-2000,2009 by Solar Designer
 *
 * ...with changes in the jumbo patch, by magnum
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */

/*
 * Configuration file loader.
 */

#ifndef _JOHN_CONFIG_H
#define _JOHN_CONFIG_H

/*
 * Parameter list entry.
 */
struct cfg_param {
	struct cfg_param *next;
	char *name, *value;
};

/*
 * Line list entry.
 */
struct cfg_line {
	struct cfg_line *next;
	char *data;
	int number;
	char *cfg_name;
	int id;
};

/*
 * Main line list structure, head is used to start scanning the list, while
 * tail is used to add new entries.
 */
struct cfg_list {
	struct cfg_line *head, *tail;
};

/*
 * Section list entry.
 */
struct cfg_section {
	struct cfg_section *next;
	char *name;
	struct cfg_param *params;
	struct cfg_list *list;
};

/*
 * Name of the currently loaded configuration file, or NULL for none.
 */
extern char *cfg_name;

/*
 * Loads a configuration file, or does nothing if one is already loaded.
 */
extern void cfg_init(char *name, int allow_missing);


/*
 * Returns a section list entry, or NULL if not found
 */
extern struct cfg_section *cfg_get_section(char *section, char *subsection);

/*
 * Searches for a section with the supplied name, and returns its line list
 * structure, or NULL if the search fails.
 */
extern struct cfg_list *cfg_get_list(char *section, char *subsection);

/*
 * Prints a list of all section names, if which == 0.
 *
 * If which == 1, only names of sections which don't contain
 * list lines (and which should contain parameters instead) get printed,
 * unless these sections are empty (no parameter definitions).
 *
 * If which == 2, only names of sections which should contain
 * list lines, not parameter definitions, get printed.
 */
void cfg_print_section_names(int which);

/*
 * Prints all the parameter definitions of a section,
 * returns the number of parameter definitions found, or -1
 * if the section doesn't exist.
 */
int cfg_print_section_params(char *section, char *subsection);

/*
 * Prints the contents of a section's list, returns the number
 * of lines printed, or -1 if the section doesn't exist.
 */
int cfg_print_section_list_lines(char *section, char *subsection);

/*
 * Searches for sections with the supplied name, and prints a list of
 * valid subsections. If function is non-null, only prints subsections
 * (ie. external modes) that has function (ie. generate or filter).
 * If notfunction is non-null, that function must NOT be present (ie.
 * for listing external modes that has filter() but not generate() )
 */
int cfg_print_subsections(char *section, char *function, char *notfunction, int print_heading);

/*
 * Searches for a section with the supplied name and a parameter within the
 * section, and returns the parameter's value, or NULL if not found.
 */
extern char *cfg_get_param(char *section, char *subsection, char *param);

/*
 * Similar to the above, but does an atoi(). Returns -1 if not found.
 */
extern int cfg_get_int(char *section, char *subsection, char *param);

/*
 * Similar to the above, takes comma-separated list, performs atoi().
 * Fills the array with values. The rest of the array is filled with -1's.
 */
extern void cfg_get_int_array(char *section, char *subsection, char *param,
		int *array, int array_len);

/*
 * Converts the value to boolean. Returns def if not found.
 */
extern int cfg_get_bool(char *section, char *subsection, char *param, int def);

#endif
