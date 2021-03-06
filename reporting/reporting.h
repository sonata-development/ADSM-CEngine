/** @file reporting.h
 * Output variables, and how they are to be reported.
 *
 * @author Neil Harvey <nharve01@uoguelph.ca><br>
 *   School of Computer Science, University of Guelph<br>
 *   Guelph, ON N1G 2W1<br>
 *   CANADA
 * @date February 2004
 *
 * Copyright &copy; University of Guelph, 2004-2009
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#ifndef REPORTING_H
#define REPORTING_H

#include <stdio.h>

#include <glib.h>



/**
 * Types of output variables.
 */
typedef enum
{
  RPT_unknown_type, RPT_integer, RPT_real, RPT_NTYPES
}
RPT_type_t;
extern const char *RPT_type_name[];



/**
 * An output variable.
 */
typedef struct
{
  char *name; /**< The variable's name.  Should not contain commas, single
    quotes ('), double quotes ("), newlines, or carriage returns. */
  RPT_type_t type; /**< The type of variable.  For variables with categories,
    this will be RPT_group; use RPT_reporting_get_type() to find the base type
    (RPT_integer or RPT_real) of the variable. */
  gboolean is_null; /**< If TRUE, this variable has no meaningful value.  Used
    in cases like the day of first detection, where there is no meaningful
    numeric value until detection occurs, or R0, where there is no meaningful
    value until 2 incubation periods have passed. */
  void *data;
}
RPT_reporting_t;



/**
 * Types of arrays of subcategories (for bulk creation of output variables).
 */
typedef enum
{
  RPT_NoSubcategory, RPT_CharArray, RPT_GPtrArray
}
RPT_subcategory_type_t;



/**
 * A structure used in bulk creation of output variables.
 */
typedef struct
{
  gpointer loc;
  const char *format;
  RPT_type_t type;
  RPT_subcategory_type_t subcategory1_type;
  gpointer subcategory1_name;
  guint subcategory1_count;
  RPT_subcategory_type_t subcategory2_type;
  gpointer subcategory2_name;
  guint subcategory2_count;
  GPtrArray *membership1;
  GPtrArray *membership2;  
}
RPT_bulk_create_t;



/* Prototypes. */

RPT_reporting_t *RPT_new_reporting (const char *name, RPT_type_t);
void RPT_free_reporting (RPT_reporting_t *);
char *RPT_reporting_to_string (RPT_reporting_t *);
char *RPT_reporting_value_to_string (RPT_reporting_t *, char *format);
int RPT_fprintf_reporting (FILE *, RPT_reporting_t *);

#define RPT_printf_reporting(R) RPT_fprintf_reporting(stdout,R)

void RPT_reporting_set_integer (RPT_reporting_t *, long);
void RPT_reporting_set_real (RPT_reporting_t *, double);
void RPT_reporting_set_null (RPT_reporting_t *);
void RPT_reporting_set_null_as_GFunc (gpointer data, gpointer user_data);
void RPT_reporting_add_integer (RPT_reporting_t *, long);
void RPT_reporting_add_real (RPT_reporting_t *, double);
#define RPT_reporting_sub_integer(R,I) RPT_reporting_add_integer(R,-(I))
#define RPT_reporting_sub_real(R,I) RPT_reporting_add_real(R,-(I))
void RPT_reporting_zero (RPT_reporting_t *);
void RPT_reporting_zero_as_GFunc (gpointer data, gpointer user_data);
long RPT_reporting_get_integer (RPT_reporting_t *);
double RPT_reporting_get_real (RPT_reporting_t *);

gboolean RPT_reporting_due (RPT_reporting_t *, unsigned int day);
RPT_type_t RPT_reporting_get_type (RPT_reporting_t *);
RPT_reporting_t *RPT_clone_reporting (RPT_reporting_t *);
void RPT_bulk_create (RPT_bulk_create_t *);

char *camelcase (char *, gboolean capitalize_first);

#endif /* !REPORTING_H */
