/** @file detection_rate_vaccination_trigger.c
 * Module that requests initiation of a vaccination program once a certain
 * number of detections (possibly restricted to a subset of production types)
 * have occurred over the last n days.
 *
 * @author nharve01@uoguelph.ca<br>
 *   School of Computer Science, University of Guelph<br>
 *   Guelph, ON N1G 2W1<br>
 *   CANADA
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

/* To avoid name clashes when multiple modules have the same interface. */
#define new detection_rate_vaccination_trigger_new
#define factory detection_rate_vaccination_trigger_factory
#define run detection_rate_vaccination_trigger_run
#define to_string detection_rate_vaccination_trigger_to_string
#define local_free detection_rate_vaccination_trigger_free
#define handle_before_each_simulation_event detection_rate_vaccination_trigger_handle_before_each_simulation_event
#define handle_new_day_event detection_rate_vaccination_trigger_handle_new_day_event
#define handle_detection_event detection_rate_vaccination_trigger_handle_detection_event
#define handle_vaccination_terminated_event detection_rate_vaccination_trigger_handle_vaccination_terminated_event

#include "module.h"
#include "module_util.h"
#include "sqlite3_exec_dict.h"
#include "detection_rate_vaccination_trigger.h"

/** This must match an element name in the DTD. */
#define MODEL_NAME "detection-rate-vaccination-trigger"



/** Specialized information for this module. */
typedef struct
{
  guint trigger_id;
  gboolean *production_type; /**< Which production types we are interested in
    counting. */
  GPtrArray *production_types; /**< Production type names. Each item in the
    array is a (char *). */
  guint num_days; /**< Number of days back we count detections */
  guint *num_detected; /**< An array of size num_days. The count of detections
    for simulation day "d" is in location (d % num_days). */
  guint current_count; /**< Maintained as a separate variable so we don't have
    to keep summing up num_detected. */
  guint threshold; /**< The number of detections at which we initiate
    vaccination. */
  GHashTable *detected; /**< Units detected so far.  Hash table used as a set
    data type to eliminate duplicates (e.g., a single unit can create detection
    events both by clinical signs and by lab test).  Keys are unit pointers
    (UNT_unit_t *), values are unimportant (presence or absence of a key is all
    we ever test. */
  gboolean applies_to_first_initiation;
  gboolean applies_to_reinitiation;
  gboolean trigger_active;
  sqlite3 *db; /* Temporarily stash a pointer to the parameters database here
    so that it will be available to the set_params function. */
}
local_data_t;



/**
 * Before each simulation, start with no detections recorded.
 *
 * @param self this module.
 */
void
handle_before_each_simulation_event (struct adsm_module_t_ *self)
{
  local_data_t *local_data;
  guint i;

  #if DEBUG
    g_debug ("----- ENTER handle_before_each_simulation_event (%s)", MODEL_NAME);
  #endif

  local_data = (local_data_t *) (self->model_data);
  g_hash_table_remove_all (local_data->detected);
  for (i = 0; i < local_data->num_days; i++)
    {
      local_data->num_detected[i] = 0;
    }
  local_data->trigger_active = local_data->applies_to_first_initiation;

  #if DEBUG
    g_debug ("----- EXIT handle_before_each_simulation_event (%s)", MODEL_NAME);
  #endif
}



/**
 * Responds to a new day event by dropping the oldest count and initializing
 * today's count to 0.
 *
 * @param self this module.
 * @param event a new day event.
 */
void
handle_new_day_event (struct adsm_module_t_ *self,
                      EVT_new_day_event_t *event)
{
  local_data_t *local_data;
  guint i;

  #if DEBUG
    g_debug ("----- ENTER handle_new_day_event (%s)", MODEL_NAME);
  #endif

  local_data = (local_data_t *) (self->model_data);
  /* Zero today's count of detected units (it replaces the oldest count in the
   * array). */
  local_data->num_detected[event->day % local_data->num_days] = 0;
  /* Initialize current_count so that we don't have to refer back to this
   * array of previous days' counts. */
  local_data->current_count = 0;
  for (i = 0; i < local_data->num_days; i++)
    {
      local_data->current_count += local_data->num_detected[i];
    }

  #if DEBUG
    g_debug ("----- EXIT handle_new_day_event (%s)", MODEL_NAME);
  #endif
  return;
}



/**
 * Responds to a detection event by counting it, and sending out an
 * InitiateVaccination event if the threshold has been crossed.
 *
 * @param self this module.
 * @param event a detection event.
 * @param queue for any new events this module creates.
 */
void
handle_detection_event (struct adsm_module_t_ *self,
                        EVT_detection_event_t *event,
                        EVT_event_queue_t *queue)
{
  local_data_t *local_data;
  UNT_unit_t *unit;

  #if DEBUG
    g_debug ("----- ENTER handle_detection_event (%s)", MODEL_NAME);
  #endif

  local_data = (local_data_t *) (self->model_data);
  unit = event->unit;
  
  /* Process this event only if this unit has not been detected before and it
   * is a production type we're interested in. */
  if (local_data->production_type[unit->production_type] == TRUE
      && g_hash_table_lookup (local_data->detected, unit) == NULL)
    {
      g_hash_table_insert (local_data->detected, unit, GINT_TO_POINTER(1));
      local_data->num_detected[event->day % local_data->num_days]++;
      local_data->current_count++;
      if (local_data->trigger_active && (local_data->current_count == local_data->threshold))
        {
          #if DEBUG
            g_debug ("%u units detected over %u days, requesting initiation of vaccination program",
                     local_data->current_count, local_data->num_days);
          #endif
          /* Note that these messages can be sent even if a vaccination program
           * is currently active. To keep the trigger modules simple, it's the
           * resources model's problem to ignore multiple/redundant requests to
           * initiate vaccination. */
          EVT_event_enqueue (queue,
            EVT_new_request_to_initiate_vaccination_event (event->day,
                                                           local_data->trigger_id,
                                                           MODEL_NAME));
        } /* end of if threshold reached */
    } /* end of if unit has not been detected before and trigger is interested in this production type */          

  #if DEBUG
    g_debug ("----- EXIT handle_detection_event (%s)", MODEL_NAME);
  #endif
  return;
}



/**
 * Responds to a vaccination terminated event by resetting the detection count
 * to 0. (Leaves the set of detected herds alone, though, since we still need
 * that to avoid re-counting a duplicate detection.)
 *
 * @param self this module.
 * @param event a vaccination terminated event.
 */
void
handle_vaccination_terminated_event (struct adsm_module_t_ *self,
                                     EVT_vaccination_terminated_event_t *event)
{
  local_data_t *local_data;
  guint i;

  #if DEBUG
    g_debug ("----- ENTER handle_vaccination_terminated_event (%s)", MODEL_NAME);
  #endif

  local_data = (local_data_t *) (self->model_data);
  for (i = 0; i < local_data->num_days; i++)
    {
      local_data->num_detected[i] = 0;
    }
  local_data->current_count = 0;
  local_data->trigger_active = local_data->applies_to_reinitiation;

  #if DEBUG
    g_debug ("----- EXIT handle_vaccination_terminated_event (%s)", MODEL_NAME);
  #endif
  return;
}



/**
 * Runs this module.
 *
 * @param self this module.
 * @param units a unit list.
 * @param zones a zone list.
 * @param event the event that caused this module to run.
 * @param rng a random number generator.
 * @param queue for any new events this module creates.
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
      handle_new_day_event (self, &(event->u.new_day));
      break;
    case EVT_Detection:
      handle_detection_event (self, &(event->u.detection), queue);
      break;
    case EVT_VaccinationTerminated:
      handle_vaccination_terminated_event (self, &(event->u.vaccination_terminated));
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
 * Returns a text representation of this module.
 *
 * @param self this module.
 * @return a string.
 */
char *
to_string (struct adsm_module_t_ *self)
{
  local_data_t *local_data;
  GString *s;
  gboolean already_names;
  guint i;

  local_data = (local_data_t *) (self->model_data);
  s = g_string_new (NULL);
  g_string_sprintf (s, "<%s for ", MODEL_NAME);
  already_names = FALSE;
  for (i = 0; i < local_data->production_types->len; i++)
    if (local_data->production_type[i] == TRUE)
      {
        if (already_names)
          g_string_append_printf (s, ",%s",
                                  (char *) g_ptr_array_index (local_data->production_types, i));
        else
          {
            g_string_append_printf (s, "%s",
                                    (char *) g_ptr_array_index (local_data->production_types, i));
            already_names = TRUE;
          }
      }
  g_string_append_printf (s, " threshold=%u in %u days", local_data->threshold,
                          local_data->num_days);
  g_string_append_c (s, '>');

  /* don't return the wrapper object */
  return g_string_free (s, /* free_segment = */ FALSE);
}



/**
 * Frees this module.
 *
 * @param self the module.
 */
void
local_free (struct adsm_module_t_ *self)
{
  local_data_t *local_data;

  #if DEBUG
    g_debug ("----- ENTER free (%s)", MODEL_NAME);
  #endif

  /* Free the dynamically-allocated parts. */
  local_data = (local_data_t *) (self->model_data);
  g_free (local_data->production_type);
  g_free (local_data->num_detected);
  g_hash_table_destroy (local_data->detected);
  g_free (local_data);
  g_ptr_array_free (self->outputs, TRUE);
  g_free (self);

  #if DEBUG
    g_debug ("----- EXIT free (%s)", MODEL_NAME);
  #endif
}



/**
 * A type to hold arguments to set_trigger_prodtype().
 */
typedef struct
{
  gboolean *flags;
  GPtrArray *production_types;
}
set_trigger_prodtype_args_t;



static int
set_trigger_prodtype (void *data, GHashTable *dict)
{
  set_trigger_prodtype_args_t *args;
  guint production_type_id;

  args = data;
  /* We receive the production type as a name (string). Get the production type
   * ID by looking it up in the array production_types. Then set the boolean
   * flag for that production type ID in the trigger. */
  production_type_id =
    adsm_read_prodtype (g_hash_table_lookup (dict, "prodtype"),
                        args->production_types);
  args->flags[production_type_id] = TRUE;
  return 0;
}



/**
 * Adds a set of parameters to a number-of-detections vaccination trigger module.
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
  sqlite3 *params;
  long int tmp;
  guint nprod_types;
  guint trigger_id;
  set_trigger_prodtype_args_t args;
  gchar *sql;
  char *sqlerr;

  #if DEBUG
    g_debug ("----- ENTER set_params (%s)", MODEL_NAME);
  #endif

  self = (adsm_module_t *)data;
  local_data = (local_data_t *) (self->model_data);
  params = local_data->db;

  local_data->threshold = strtol(g_hash_table_lookup (dict, "number_of_units"), NULL, /* base */ 10);
  local_data->num_days = strtol(g_hash_table_lookup (dict, "days"), NULL, /* base */ 10);

  /* An array to hold per-day detection counts over the last num_days days. */
  local_data->num_detected = g_new (guint, local_data->num_days);

  errno = 0;
  tmp = strtol (g_hash_table_lookup (dict, "restart_only"), NULL, /* base */ 10);
  g_assert (errno != ERANGE && errno != EINVAL);
  g_assert (tmp == 0 || tmp == 1);
  local_data->applies_to_first_initiation = (tmp == 0);
  local_data->applies_to_reinitiation = (tmp == 1);

  /* Fill in the production types that this trigger counts. */
  nprod_types = local_data->production_types->len;
  local_data->production_type = g_new(gboolean, nprod_types);
  trigger_id = strtol(g_hash_table_lookup (dict, "id"), NULL, /* base */ 10);
  sql = g_strdup_printf ("SELECT prodtype.name AS prodtype "
                         "FROM ScenarioCreator_productiontype prodtype,ScenarioCreator_rateofnewdetections_trigger_group trigger "
                         "WHERE trigger.rateofnewdetections_id=%u "
                         "AND prodtype.id=trigger.productiontype_id",
                         trigger_id);
  args.flags = local_data->production_type;
  args.production_types = local_data->production_types;
  sqlite3_exec_dict (params, sql,
                     set_trigger_prodtype, &args, &sqlerr);
  if (sqlerr)
    {
      g_error ("%s", sqlerr);
    }
  g_free (sql);

  #if DEBUG
    g_debug ("----- EXIT set_params (%s)", MODEL_NAME);
  #endif

  return 0;
}



/**
 * Returns a new detection-rate vaccination trigger module.
 */
adsm_module_t *
new (sqlite3 * params, UNT_unit_list_t * units, projPJ projection,
     ZON_zone_list_t * zones, gpointer user_data, GError **error)
{
  adsm_module_t *self;
  local_data_t *local_data;
  EVT_event_type_t events_listened_for[] = {
    EVT_BeforeEachSimulation,
    EVT_NewDay,
    EVT_Detection,
    EVT_VaccinationTerminated,
    0
  };
  guint trigger_id;
  char *sql;
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

  local_data->production_types = units->production_type_names;

  /* A hash table (used as set data type) for counting unique detected units. */
  local_data->detected = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Call the set_params function to read trigger's details. */
  trigger_id = GPOINTER_TO_UINT(user_data);
  local_data->trigger_id = trigger_id;
  sql = g_strdup_printf ("SELECT id,number_of_units,days,restart_only "
                         "FROM ScenarioCreator_rateofnewdetections "
                         "WHERE id=%u", trigger_id);
  local_data->db = params;
  sqlite3_exec_dict (params, sql, set_params, self, &sqlerr);
  if (sqlerr)
    {
      g_error ("%s", sqlerr);
    }
  local_data->db = NULL;
  g_free (sql);

  #if DEBUG
    g_debug ("----- EXIT new (%s)", MODEL_NAME);
  #endif

  return self;
}



/**
 * A type to hold arguments to create_trigger().
 */
typedef struct
{
  sqlite3 *params;
  UNT_unit_list_t *units;
  projPJ projection;
  ZON_zone_list_t * zones;
  GError **error;
  GSList *triggers;  
}
create_trigger_args_t;



/**
 * The function receives a trigger ID and calls "new" to instantiate that
 * trigger.
 */
static int
create_trigger (void *data, GHashTable *dict)
{
  create_trigger_args_t *args;
  guint trigger_id;
  adsm_module_t *module;

  args = data;
  
  /* We receive the trigger ID as a name (string). */
  trigger_id = strtol(g_hash_table_lookup (dict, "id"), NULL, /* base */ 10);
  #if DEBUG
    g_debug ("creating trigger with ID %u", trigger_id);
  #endif

  /* Call the "new" function to instantiate a module for the trigger with that
   * ID. */
  module =
    detection_rate_vaccination_trigger_new (args->params, args->units,
      args->projection, args->zones, GUINT_TO_POINTER(trigger_id), args->error);

  /* Add the new module to the list returned by the factory. */
  args->triggers = g_slist_prepend (args->triggers, module);

  return 0;
}



/**
 * Returns one or more new detection-rate vaccination trigger module.
 */
GSList *
factory (sqlite3 * params, UNT_unit_list_t * units, projPJ projection,
         ZON_zone_list_t * zones, GError **error)
{
  GSList *modules;
  create_trigger_args_t args;
  char *sqlerr;

  #if DEBUG
    g_debug ("----- ENTER factory (%s)", MODEL_NAME);
  #endif

  /* Call the set_params function to read the individual triggers. Because
   * sqlite3_exec_dict callback functions have only 1 parameter available for
   * user data, we pack all the arguments set_params will need into a structure
   * to pass. */
  args.params = params;
  args.units = units;
  args.projection = projection;
  args.zones = zones;
  args.error = error;
  args.triggers = NULL;
  sqlite3_exec_dict (params,
                     "SELECT id FROM ScenarioCreator_rateofnewdetections",
                     create_trigger, &args, &sqlerr);
  if (sqlerr)
    {
      g_error ("%s", sqlerr);
    }
  modules = args.triggers;

  #if DEBUG
    g_debug ("----- EXIT factory (%s)", MODEL_NAME);
  #endif

  return modules;
}

/* end of file detection_rate_vaccination_trigger.c */
