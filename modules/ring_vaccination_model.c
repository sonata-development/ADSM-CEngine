/** @file ring_vaccination_model.c
 * Module that simulates a policy of vaccinating units within a certain
 * distance of a diseased unit.
 *
 * When a unit is detected as diseased, this module requests the vaccination of
 * all units within a given radius of the diseased unit.
 *
 * @author Neil Harvey <nharve01@uoguelph.ca><br>
 *   School of Computer Science, University of Guelph<br>
 *   Guelph, ON N1G 2W1<br>
 *   CANADA
 * @date September 2003
 *
 * Copyright &copy; University of Guelph, 2003-2009
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#if HAVE_CONFIG_H
#  include "config.h"
#endif

/* To avoid name clashes when multiple modules have the same interface. */
#define new ring_vaccination_model_new
#define run ring_vaccination_model_run
#define to_string ring_vaccination_model_to_string
#define local_free ring_vaccination_model_free
#define handle_before_each_simulation_event ring_vaccination_model_handle_before_each_simulation_event
#define handle_new_day_event ring_vaccination_model_handle_new_day_event
#define handle_detection_event ring_vaccination_model_handle_detection_event
#define check_and_choose ring_vaccination_model_check_and_choose

#include "module.h"
#include "module_util.h"
#include "gis.h"
#include "spatial_search.h"
#include "sqlite3_exec_dict.h"

#if STDC_HEADERS
#  include <string.h>
#endif

#if HAVE_STRINGS_H
#  include <strings.h>
#endif

#if HAVE_MATH_H
#  include <math.h>
#endif

#include "ring_vaccination_model.h"

#if !HAVE_ROUND && HAVE_RINT
#  define round rint
#endif

/* Temporary fix -- "round" and "rint" are in the math library on Red Hat 7.3,
 * but they're #defined so AC_CHECK_FUNCS doesn't find them. */
double round (double x);

/** This must match an element name in the DTD. */
#define MODEL_NAME "ring-vaccination-model"



#define EPSILON 0.01 /* 10 m */



/** Specialized information for this model. */
typedef struct
{
  int priority;
  unsigned int min_time_between_vaccinations; /**< The minimum number of days
    until a unit may be revaccinated. */
  double radius; /**< The radius of ring created around a unit of this
    production type. If negative, no ring is created around units of this
    production type. */
  gboolean vaccinate_detected_units;
}
param_block_t;



typedef struct
{
  GPtrArray *production_types;
  param_block_t **param_block; /**< Blocks of parameters.
    Use an expression of the form
    param_block[production_type]
    to get a pointer to a particular param_block. */
  GHashTable *detected_units; /**< A list of detected units.  The pointer to
    the UNT_unit_t structure is the key. */
  GHashTable *requested_today; /**< A list of units for which this module made
    requests for vaccination today.  A unit can be in queue for vaccination
    more than once at a given time, but two requests for the same unit, for the
    same reason, on the same day will have identical priority and would be
    redundant. */      
}
local_data_t;



/**
 * Before each simulation, this module deletes any records of detected units
 * left over from a previous iteration.
 *
 * @param self the model.
 */
void
handle_before_each_simulation_event (struct adsm_module_t_ *self)
{
  local_data_t *local_data;

  #if DEBUG
    g_debug ("----- ENTER handle_before_each_simulation_event (%s)", MODEL_NAME);
  #endif

  local_data = (local_data_t *) (self->model_data);
  g_hash_table_remove_all (local_data->detected_units);

  #if DEBUG
    g_debug ("----- EXIT handle_before_each_simulation_event (%s)", MODEL_NAME);
  #endif

  return;
}



/**
 * On each new day, this module clears its list of units for which it has
 * requested vaccinations.
 *
 * @param self this module.
 */
void
handle_new_day_event (struct adsm_module_t_ *self)
{
  local_data_t *local_data;
  
#if DEBUG
  g_debug ("----- ENTER handle_new_day_event (%s)", MODEL_NAME);
#endif
    
  local_data = (local_data_t *) (self->model_data);
  g_hash_table_remove_all (local_data->requested_today);

#if DEBUG
  g_debug ("----- EXIT handle_new_day_event (%s)", MODEL_NAME);
#endif

  return;
}



/**
 * Special structure for use with the callback function below.
 */
typedef struct
{
  local_data_t *local_data;
  UNT_unit_list_t *units;
  UNT_unit_t *unit1;
  int day;
  EVT_event_queue_t *queue;
} callback_t;



/**
 * Check whether unit 2 should be part of the vaccination ring.
 */
void
check_and_choose (int id, gpointer arg)
{
  callback_t *callback_data;
  UNT_unit_t *unit2;
  UNT_unit_t *unit1;
  local_data_t *local_data;
  param_block_t *param_block;

#if DEBUG
  g_debug ("----- ENTER check_and_choose (%s)", MODEL_NAME);
#endif

  callback_data = (callback_t *) arg;
  unit2 = UNT_unit_list_get (callback_data->units, id);

  /* Is unit 2 already destroyed? */
  if (unit2->state == Destroyed)
    goto end;

  local_data = callback_data->local_data;
  unit1 = callback_data->unit1;

  /* Is unit 2 a production type that gets vaccinated? */
  param_block = local_data->param_block[unit2->production_type];
  if (param_block == NULL || param_block->priority == INT_MAX)
    goto end;

  /* Do we want to exclude units that are known to be infected? */
  if (param_block->vaccinate_detected_units == FALSE)
    {
      /* We know unit2 is infected if it's the one that triggered this
       * vaccination ring, or if it has been detected. */
      if (unit1 == unit2
          || g_hash_table_lookup (local_data->detected_units, unit2) != NULL)
        goto end;
    }

  /* Avoid making the same request twice on a single day. */
  if (g_hash_table_lookup (local_data->requested_today, unit2) != NULL)
    goto end;

#if DEBUG
  g_debug ("unit \"%s\" within radius, ordering unit vaccinated", unit2->official_id);
#endif
  EVT_event_enqueue (callback_data->queue,
                     EVT_new_request_for_vaccination_event (unit2,
                                                            callback_data->day,
                                                            ADSM_ControlRing,
                                                            param_block->priority,
                                                            !(param_block->vaccinate_detected_units),
                                                            param_block->min_time_between_vaccinations));
  g_hash_table_insert (local_data->requested_today, unit2, unit2);

end:
#if DEBUG
  g_debug ("----- EXIT check_and_choose (%s)", MODEL_NAME);
#endif
  return;
}



void
ring_vaccinate (struct adsm_module_t_ *self, UNT_unit_list_t * units, UNT_unit_t * unit,
                int day, EVT_event_queue_t * queue)
{
  local_data_t *local_data;
  param_block_t *param_block;
  callback_t callback_data;

#if DEBUG
  g_debug ("----- ENTER ring_vaccinate (%s)", MODEL_NAME);
#endif

  local_data = (local_data_t *) (self->model_data);

  callback_data.local_data = local_data;
  callback_data.units = units;
  callback_data.unit1 = unit;
  callback_data.day = day;
  callback_data.queue = queue;

  /* Find the distances to other units. */
  param_block = local_data->param_block[unit->production_type];
  spatial_search_circle_by_id (units->spatial_index, unit->index,
                               param_block->radius + EPSILON,
                               check_and_choose, &callback_data);

#if DEBUG
  g_debug ("----- EXIT ring_vaccinate (%s)", MODEL_NAME);
#endif
}



/**
 * Responds to a detection by ordering vaccination actions.
 *
 * @param self the model.
 * @param units a list of units.
 * @param event a detection event.
 * @param queue for any new events the model creates.
 */
void
handle_detection_event (struct adsm_module_t_ *self, UNT_unit_list_t * units,
                        EVT_detection_event_t * event, EVT_event_queue_t * queue)
{
  local_data_t *local_data;
  UNT_unit_t *unit;
  param_block_t *param_block;
  
#if DEBUG
  g_debug ("----- ENTER handle_detection_event (%s)", MODEL_NAME);
#endif
    
  local_data = (local_data_t *) (self->model_data);
  unit = event->unit;
  g_hash_table_insert (local_data->detected_units, (gpointer)unit, (gpointer)unit);

  param_block = local_data->param_block[unit->production_type];
  if (param_block != NULL && param_block->radius >= 0)
    ring_vaccinate (self, units, unit, event->day, queue);

#if DEBUG
  g_debug ("----- EXIT handle_detection_event (%s)", MODEL_NAME);
#endif
}



/**
 * Runs this model.
 *
 * @param self the model.
 * @param units a unit list.
 * @param zones a zone list.
 * @param event the event that caused the model to run.
 * @param rng a random number generator.
 * @param queue for any new events the model creates.
 */
void
run (struct adsm_module_t_ *self, UNT_unit_list_t * units, ZON_zone_list_t * zones,
     EVT_event_t * event, RAN_gen_t * rng, EVT_event_queue_t * queue)
{
#if DEBUG
  g_debug ("----- ENTER run (%s)", MODEL_NAME);
#endif

  switch (event->type)
    {
    case EVT_BeforeEachSimulation:
      handle_before_each_simulation_event (self);
      break;
    case EVT_NewDay:
      handle_new_day_event (self);
      break;
    case EVT_Detection:
      handle_detection_event (self, units, &(event->u.detection), queue);
      break;
    default:
      g_error
        ("%s has received a %s event, which it does not listen for.  This should never happen.  Please contact the developer.",
         MODEL_NAME, EVT_event_type_name[event->type]);
    }

#if DEBUG
  g_debug ("----- EXIT run (%s)", MODEL_NAME);
#endif
}



/**
 * Returns a text representation of this model.
 *
 * @param self the model.
 * @return a string.
 */
char *
to_string (struct adsm_module_t_ *self)
{
  GString *s;
  local_data_t *local_data;
  unsigned int nprod_types, i;
  param_block_t *param_block;
  char *chararray;

  local_data = (local_data_t *) (self->model_data);
  s = g_string_new (NULL);
  g_string_printf (s, "<%s", MODEL_NAME);

  /* Add the parameter block for each production type. */
  nprod_types = local_data->production_types->len;
  for (i = 0; i < nprod_types; i++)
    {
      param_block = local_data->param_block[i];
      if (param_block != NULL)
        {
          g_string_append_printf (s, "\n  for %s",
                                  (char *) g_ptr_array_index (local_data->production_types, i));
          g_string_append_printf (s, "\n    radius=%g", param_block->radius);
		  if (param_block->priority < INT_MAX)
		    {
              g_string_append_printf (s, "\n    priority=%i", param_block->priority);
              g_string_append_printf (s, "\n    min-time-between-vaccinations=%u",
                                      param_block->min_time_between_vaccinations);
            }
        }
    }
  g_string_append_c (s, '>');

  /* don't return the wrapper object */
  chararray = s->str;
  g_string_free (s, FALSE);
  return chararray;
}



/**
 * Frees this model.  Does not free the production type names.
 *
 * @param self the model.
 */
void
local_free (struct adsm_module_t_ *self)
{
  local_data_t *local_data;
  unsigned int nprod_types, i;

#if DEBUG
  g_debug ("----- ENTER free (%s)", MODEL_NAME);
#endif

  local_data = (local_data_t *) (self->model_data);

  /* Free each of the parameter blocks. */
  nprod_types = local_data->production_types->len;
  for (i = 0; i < nprod_types; i++)
    {
      if (local_data->param_block[i] != NULL)
        g_free (local_data->param_block[i]);
    }
  /* Free the array of pointers. */
  g_free (local_data->param_block);

  g_hash_table_destroy (local_data->detected_units);
  g_hash_table_destroy (local_data->requested_today);
  g_free (local_data);
  g_ptr_array_free (self->outputs, TRUE);
  g_free (self);

#if DEBUG
  g_debug ("----- EXIT free (%s)", MODEL_NAME);
#endif
}



/**
 * Adds a set of parameters to a ring vaccination model.
 *
 * @param data this module ("self"), but cast to a void *.
 * @param dict the SQL query result as a GHashTable in which key = colname,
 *   value = value, both in (char *) format.
 * @return 0
 */
static int
set_params (void *data, GHashTable *dict)
{
  adsm_module_t *self;
  local_data_t *local_data;
  guint production_type_id;
  param_block_t *p;
  long int tmp;

#if DEBUG
  g_debug ("----- ENTER set_params (%s)", MODEL_NAME);
#endif

  g_assert (g_hash_table_size (dict) == 7);

  self = (adsm_module_t *)data;
  local_data = (local_data_t *) (self->model_data);

  /* Find out which production types these parameters apply to. */
  production_type_id =
    adsm_read_prodtype (g_hash_table_lookup (dict, "prodtype"),
                        local_data->production_types);

  /* Check that we are not overwriting an existing parameter block (that would
   * indicate a bug). */
  g_assert (local_data->param_block[production_type_id] == NULL);

  /* Create a new parameter block. */
  p = g_new (param_block_t, 1);
  local_data->param_block[production_type_id] = p;

  /* Read the parameters. */
  errno = 0;
  tmp = strtol (g_hash_table_lookup (dict, "trigger_vaccination_ring"), NULL, /* base */ 10);
  g_assert (errno != ERANGE && errno != EINVAL);  
  g_assert (tmp == 0 || tmp == 1);
  if (tmp == 1)
    {
      errno = 0;
      p->radius = strtod (g_hash_table_lookup (dict, "vaccination_ring_radius"), NULL);
      g_assert (errno != ERANGE);
      /* Radius must be positive. */
      if (p->radius < 0)
        {
          g_warning ("%s: radius cannot be negative, setting to 0", MODEL_NAME);
          p->radius = 0;
        }
    }
  else
    {
      /* Do not vaccinate around detected units of this type. */
      p->radius = -1;
    }

  errno = 0;
  tmp = strtol (g_hash_table_lookup (dict, "use_vaccination"), NULL, /* base */ 10);
  g_assert (errno != ERANGE && errno != EINVAL);
  g_assert (tmp == 0 || tmp == 1);
  if (tmp == 1)
    {
      tmp = strtol (g_hash_table_lookup (dict, "vaccination_priority"), NULL, /* base */ 10);
      g_assert (errno != ERANGE && errno != EINVAL);
      p->priority = tmp;
      if (p->priority < 1)
        {
          g_warning ("%s: priority cannot be less than 1, setting to 1", MODEL_NAME);
          p->priority = 1;
        }

      tmp = strtol (g_hash_table_lookup (dict, "vaccinate_detected_units"), NULL, /* base */ 10);
      g_assert (errno != ERANGE && errno != EINVAL);
      g_assert (tmp == 0 || tmp == 1);
      p->vaccinate_detected_units = (tmp == 1);

      tmp = strtol (g_hash_table_lookup (dict, "minimum_time_between_vaccinations"), NULL, /* base */ 10);
      g_assert (errno != ERANGE && errno != EINVAL);
      if (tmp < 1)
        {
          g_warning ("%s: minimum time between vaccinations cannot be less than 1, setting to 1", MODEL_NAME);
          tmp = 1;
        }
      p->min_time_between_vaccinations = tmp;
    }
  else
    {
      /* Do not vaccinate units of this type. */
      p->priority = INT_MAX;
      p->vaccinate_detected_units = FALSE;
      p->min_time_between_vaccinations = 0;
    }

#if DEBUG
  g_debug ("----- EXIT set_params (%s)", MODEL_NAME);
#endif

  return 0;
}



/**
 * Returns a new ring vaccination model.
 */
adsm_module_t *
new (sqlite3 * params, UNT_unit_list_t * units, projPJ projection,
     ZON_zone_list_t * zones, GError **error)
{
  adsm_module_t *self;
  local_data_t *local_data;
  EVT_event_type_t events_listened_for[] = {
    EVT_BeforeEachSimulation,
    EVT_NewDay,
    EVT_Detection,
    0
  };
  unsigned int nprod_types;
  char *sqlerr;

#if DEBUG
  g_debug ("----- ENTER new (%s)", MODEL_NAME);
#endif

  self = g_new (adsm_module_t, 1);
  local_data = g_new (local_data_t, 1);

  self->name = MODEL_NAME;
  self->events_listened_for = adsm_setup_events_listened_for (events_listened_for);
  self->outputs = g_ptr_array_new ();
  self->model_data = local_data;
  self->run = run;
  self->is_listening_for = adsm_model_is_listening_for;
  self->has_pending_actions = adsm_model_answer_no;
  self->has_pending_infections = adsm_model_answer_no;
  self->to_string = to_string;
  self->printf = adsm_model_printf;
  self->fprintf = adsm_model_fprintf;
  self->free = local_free;

  /* local_data->param_block is an array of parameter blocks, each block
   * holding the parameters for one production type. Initially, all pointers
   * are NULL.  Parameter blocks will be created as needed in the set_params
   * function. */
  local_data->production_types = units->production_type_names;
  nprod_types = local_data->production_types->len;
  local_data->param_block = g_new0 (param_block_t *, nprod_types);

  /* Initialize a list of detected units. */
  local_data->detected_units = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Initialize a list of units for avoiding making two requests for the same
   * unit on the same day. */
  local_data->requested_today = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Call the set_params function to read the production type combination
   * specific parameters. */
  sqlite3_exec_dict (params,
                     "SELECT prodtype.name AS prodtype,trigger_vaccination_ring,vaccination_ring_radius,use_vaccination,vaccination_priority,vaccinate_detected_units,minimum_time_between_vaccinations "
                     "FROM ScenarioCreator_productiontype prodtype,ScenarioCreator_controlprotocol vaccine,ScenarioCreator_protocolassignment xref "
                     "WHERE prodtype.id=xref.production_type_id "
                     "AND xref.control_protocol_id=vaccine.id "
                     "AND (trigger_vaccination_ring=1 OR use_vaccination=1)",
                     set_params, self, &sqlerr);
  if (sqlerr)
    {
      g_error ("%s", sqlerr);
    }

#if DEBUG
  g_debug ("----- EXIT new (%s)", MODEL_NAME);
#endif

  return self;
}

/* end of file ring_vaccination_model.c */
