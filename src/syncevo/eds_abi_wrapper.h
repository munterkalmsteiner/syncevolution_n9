/*
 * Copyright (C) 2005-2009 Patrick Ohly <patrick.ohly@gmx.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

/**
 * The purpose of this file is to separate SyncEvolution from
 * Evolution Dataserver ABI changes by never depending directly
 * on any symbol in its libraries. Instead all functions are
 * called via function pointers found via dlopen/dlsym.
 *
 * This is more flexible than linking against a specific revision of
 * the libs, but circumvents the usual lib versioning and therefore
 * may fail when the functions needed by SyncEvolution change.
 *
 * History shows that this has not happened for a long time whereas
 * the version of libs had to be bumped quite a few times due to other
 * changes.
 *
 * To use the wrappers, include this header file. It overrides
 * the normal C API functions with the function pointers via
 * defines.
 */

#ifndef INCL_EDS_ABI_WRAPPER
#define INCL_EDS_ABI_WRAPPER

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_EDS
#include <glib-object.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-list.h>
#ifdef ENABLE_EBOOK
#include <libebook/e-book.h>
#include <libebook/e-vcard.h>
#endif
#ifdef ENABLE_ECAL
# define HANDLE_LIBICAL_MEMORY 1
#include <libecal/e-cal.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** libebook, libecal, libedataserver available (currently checks for e_book_new/e_cal_new/e_source_group_peek_sources) */
extern int EDSAbiHaveEbook, EDSAbiHaveEcal, EDSAbiHaveEdataserver;

#ifdef EVOLUTION_COMPATIBILITY

/**
 * This is a struct instead of a namespace because that allows
 * printing it in a debugger. This code also has to be usable
 * in plain C.
 */
struct EDSAbiWrapper {
# ifdef HAVE_EDS
    GType (*e_source_get_type) (void);
    char *(*e_source_get_uri) (ESource *source);
    GType (*e_source_group_get_type) (void);
    GSList *(*e_source_group_peek_sources) (ESourceGroup *group);
    GSList *(*e_source_list_peek_groups) (ESourceList *list);
    const char *(*e_source_peek_name) (ESource *source);
# endif /* HAVE_EDS */

# ifdef ENABLE_EBOOK
    gboolean (*e_book_add_contact) (EBook *book, EContact *contact, GError **error);
    gboolean (*e_book_authenticate_user) (EBook *book, const char *user, const char *passwd, const char *auth_method, GError **error);
    gboolean (*e_book_commit_contact) (EBook *book, EContact *contact, GError **error);
    EContact* (*e_contact_duplicate) (EContact *contact);
    gconstpointer (*e_contact_get_const) (EContact *contact, EContactField field_id);
    gpointer (*e_contact_get) (EContact *contact, EContactField field_id);
    void (*e_contact_name_free)(EContactName *name);
    GType (*e_contact_get_type) (void);
    EContact* (*e_contact_new_from_vcard) (const char *vcard);
    void (*e_contact_set) (EContact *contact, EContactField field_id, const gpointer value);
    gboolean (*e_book_get_addressbooks) (ESourceList** addressbook_sources, GError **error);
    gboolean (*e_book_get_changes) (EBook *book, char *changeid, GList **changes, GError **error);
    gboolean (*e_book_get_contact) (EBook *book, const char *id, EContact **contact, GError **error);
    gboolean (*e_book_get_contacts) (EBook *book, EBookQuery *query, GList **contacts, GError **error);
    gboolean (*e_book_get_supported_auth_methods) (EBook *book, GList **auth_methods, GError **error);
    const char *(*e_book_get_uri) (EBook *book);
    EBook *(*e_book_new) (ESource *source, GError **error);
    EBook *(*e_book_new_default_addressbook) (GError **error);
    EBook *(*e_book_new_from_uri) (const char *uri, GError **error);
    EBook *(*e_book_new_system_addressbook) (GError **error);
    gboolean (*e_book_open) (EBook *book, gboolean only_if_exists, GError **error);
    EBookQuery* (*e_book_query_any_field_contains) (const char  *value);
    void (*e_book_query_unref) (EBookQuery *q);
    GQuark (*e_book_error_quark) (void);
    gboolean (*e_book_remove_contact) (EBook *book, const char *id, GError **error);
    char* (*e_vcard_to_string) (EVCard *evc, EVCardFormat format);
# endif /* ENABLE_EBOOK */

# ifdef ENABLE_ECAL
    gboolean (*e_cal_add_timezone) (ECal *ecal, icaltimezone *izone, GError **error);
    icalcomponent *(*e_cal_component_get_icalcomponent) (ECalComponent *comp);
    void (*e_cal_component_get_last_modified) (ECalComponent *comp, struct icaltimetype **t);
    GType (*e_cal_component_get_type) (void);
    gboolean (*e_cal_create_object) (ECal *ecal, icalcomponent *icalcomp, char **uid, GError **error);
    GQuark (*e_calendar_error_quark) (void) G_GNUC_CONST;
    char* (*e_cal_get_component_as_string) (ECal *ecal, icalcomponent *icalcomp);
    gboolean (*e_cal_get_object) (ECal *ecal, const char *uid, const char *rid, icalcomponent **icalcomp, GError **error);
    gboolean (*e_cal_get_object_list_as_comp) (ECal *ecal, const char *query, GList **objects, GError **error);
    gboolean (*e_cal_get_sources) (ESourceList **sources, ECalSourceType type, GError **error);
    gboolean (*e_cal_get_timezone) (ECal *ecal, const char *tzid, icaltimezone **zone, GError **error);
    gboolean (*e_cal_modify_object) (ECal *ecal, icalcomponent *icalcomp, CalObjModType mod, GError **error);
    ECal *(*e_cal_new) (ESource *source, ECalSourceType type);
    ECal *(*e_cal_new_from_uri) (const gchar *uri, ECalSourceType type);
    ECal *(*e_cal_new_system_calendar) (void);
    ECal *(*e_cal_new_system_tasks) (void);
    const gchar *(*e_cal_get_uri)(ECal *);
    gboolean (*e_cal_open) (ECal *ecal, gboolean only_if_exists, GError **error);
    gboolean (*e_cal_remove_object) (ECal *ecal, const char *uid, GError **error);
    gboolean (*e_cal_remove_object_with_mod) (ECal *ecal, const char *uid, const char *rid, CalObjModType mod, GError **error);
    void (*e_cal_set_auth_func) (ECal *ecal, ECalAuthFunc func, gpointer data);
    void (*icalcomponent_add_component) (icalcomponent* parent, icalcomponent* child);
    char* (*icalcomponent_as_ical_string) (icalcomponent* component);
    void (*icalcomponent_free) (icalcomponent* component);
    icalcomponent* (*icalcomponent_get_first_component) (icalcomponent* component, icalcomponent_kind kind);
    icalproperty* (*icalcomponent_get_first_property) (icalcomponent* component, icalproperty_kind kind);
    icalcomponent* (*icalcomponent_get_next_component) (icalcomponent* component, icalcomponent_kind kind);
    icalproperty* (*icalcomponent_get_next_property) (icalcomponent* component, icalproperty_kind kind);
    struct icaltimetype (*icalcomponent_get_recurrenceid) (icalcomponent* comp);
    icaltimezone* (*icalcomponent_get_timezone) (icalcomponent* comp, const char *tzid);
    const char* (*icalcomponent_get_uid) (icalcomponent* comp);
    icalcomponent_kind (*icalcomponent_isa) (const icalcomponent* component);
    icalcomponent* (*icalcomponent_new_clone) (icalcomponent* component);
    icalcomponent* (*icalcomponent_new_from_string) (char* str);
    void (*icalcomponent_remove_property) (icalcomponent* component, icalproperty* property);
    void (*icalcomponent_set_uid) (icalcomponent* comp, const char* v);
    icalcomponent* (*icalcomponent_vanew) (icalcomponent_kind kind, ...);
    const char* (*icalparameter_get_tzid) (const icalparameter* value);
    void (*icalparameter_set_tzid) (icalparameter* value, const char* v);
    const char* (*icalproperty_get_description) (const icalproperty* prop);
    icalparameter* (*icalproperty_get_first_parameter) (icalproperty* prop, icalparameter_kind kind);
    struct icaltimetype (*icalproperty_get_lastmodified) (const icalproperty* prop);
    icalproperty* (*icalproperty_vanew_lastmodified) (struct icaltimetype v, ...);
    icalparameter* (*icalproperty_get_next_parameter) (icalproperty* prop, icalparameter_kind kind);
    const char* (*icalproperty_get_summary) (const icalproperty* prop);
    icalproperty* (*icalproperty_new_description) (const char* v);
    icalproperty* (*icalproperty_new_summary) (const char* v);
    void (*icalproperty_set_value_from_string) (icalproperty* prop,const char* value, const char* kind);
    const char* (*icaltime_as_ical_string) (const struct icaltimetype tt);

    void (*icaltimezone_free) (icaltimezone *zone, int free_struct);
    icaltimezone* (*icaltimezone_get_builtin_timezone) (const char *location);
    icaltimezone* (*icaltimezone_get_builtin_timezone_from_tzid) (const char *tzid);
    icalcomponent* (*icaltimezone_get_component) (icaltimezone *zone);
    char* (*icaltimezone_get_tzid) (icaltimezone *zone);
    icaltimezone *(*icaltimezone_new) (void);
    int (*icaltimezone_set_component) (icaltimezone *zone, icalcomponent *comp);
# endif /* ENABLE_ECAL */

    int initialized;
};

extern struct EDSAbiWrapper EDSAbiWrapperSingleton;

# ifndef EDS_ABI_WRAPPER_NO_REDEFINE
#  ifdef HAVE_EDS
#   define e_source_get_type EDSAbiWrapperSingleton.e_source_get_type
#   define e_source_get_uri EDSAbiWrapperSingleton.e_source_get_uri
#   define e_source_group_get_type EDSAbiWrapperSingleton.e_source_group_get_type
#   define e_source_group_peek_sources EDSAbiWrapperSingleton.e_source_group_peek_sources
#   define e_source_list_peek_groups EDSAbiWrapperSingleton.e_source_list_peek_groups
#   define e_source_peek_name EDSAbiWrapperSingleton.e_source_peek_name
#  endif /* HAVE_EDS */

#  ifdef ENABLE_EBOOK
#   define e_book_add_contact EDSAbiWrapperSingleton.e_book_add_contact
#   define e_book_authenticate_user EDSAbiWrapperSingleton.e_book_authenticate_user
#   define e_book_commit_contact EDSAbiWrapperSingleton.e_book_commit_contact
#   define e_contact_duplicate EDSAbiWrapperSingleton.e_contact_duplicate
#   define e_contact_get_const EDSAbiWrapperSingleton.e_contact_get_const
#   define e_contact_get EDSAbiWrapperSingleton.e_contact_get
#   define e_contact_name_free EDSAbiWrapperSingleton.e_contact_name_free
#   define e_contact_get_type EDSAbiWrapperSingleton.e_contact_get_type
#   define e_contact_new_from_vcard EDSAbiWrapperSingleton.e_contact_new_from_vcard
#   define e_contact_set EDSAbiWrapperSingleton.e_contact_set
#   define e_book_error_quark EDSAbiWrapperSingleton.e_book_error_quark
#   define e_book_get_addressbooks EDSAbiWrapperSingleton.e_book_get_addressbooks
#   define e_book_get_changes EDSAbiWrapperSingleton.e_book_get_changes
#   define e_book_get_contact EDSAbiWrapperSingleton.e_book_get_contact
#   define e_book_get_contacts EDSAbiWrapperSingleton.e_book_get_contacts
#   define e_book_get_supported_auth_methods EDSAbiWrapperSingleton.e_book_get_supported_auth_methods
#   define e_book_get_uri EDSAbiWrapperSingleton.e_book_get_uri
#   define e_book_new EDSAbiWrapperSingleton.e_book_new
#   define e_book_new_default_addressbook EDSAbiWrapperSingleton.e_book_new_default_addressbook
#   define e_book_new_from_uri EDSAbiWrapperSingleton.e_book_new_from_uri
#   define e_book_new_system_addressbook EDSAbiWrapperSingleton.e_book_new_system_addressbook
#   define e_book_open EDSAbiWrapperSingleton.e_book_open
#   define e_book_query_any_field_contains EDSAbiWrapperSingleton.e_book_query_any_field_contains
#   define e_book_query_unref EDSAbiWrapperSingleton.e_book_query_unref
#   define e_book_remove_contact EDSAbiWrapperSingleton.e_book_remove_contact
#   define e_vcard_to_string EDSAbiWrapperSingleton.e_vcard_to_string
#  endif /* ENABLE_EBOOK */

#  ifdef ENABLE_ECAL
#   define e_cal_add_timezone EDSAbiWrapperSingleton.e_cal_add_timezone
#   define e_cal_component_get_icalcomponent EDSAbiWrapperSingleton.e_cal_component_get_icalcomponent
#   define e_cal_component_get_last_modified EDSAbiWrapperSingleton.e_cal_component_get_last_modified
#   define e_cal_component_get_type EDSAbiWrapperSingleton.e_cal_component_get_type
#   define e_cal_create_object EDSAbiWrapperSingleton.e_cal_create_object
#   define e_calendar_error_quark EDSAbiWrapperSingleton.e_calendar_error_quark
#   define e_cal_get_component_as_string EDSAbiWrapperSingleton.e_cal_get_component_as_string
#   define e_cal_get_object EDSAbiWrapperSingleton.e_cal_get_object
#   define e_cal_get_object_list_as_comp EDSAbiWrapperSingleton.e_cal_get_object_list_as_comp
#   define e_cal_get_sources EDSAbiWrapperSingleton.e_cal_get_sources
#   define e_cal_get_timezone EDSAbiWrapperSingleton.e_cal_get_timezone
#   define e_cal_modify_object EDSAbiWrapperSingleton.e_cal_modify_object
#   define e_cal_new EDSAbiWrapperSingleton.e_cal_new
#   define e_cal_new_from_uri EDSAbiWrapperSingleton.e_cal_new_from_uri
#   define e_cal_new_system_calendar EDSAbiWrapperSingleton.e_cal_new_system_calendar
#   define e_cal_new_system_tasks EDSAbiWrapperSingleton.e_cal_new_system_tasks
#   define e_cal_get_uri EDSAbiWrapperSingleton.e_cal_get_uri
#   define e_cal_open EDSAbiWrapperSingleton.e_cal_open
#   define e_cal_remove_object EDSAbiWrapperSingleton.e_cal_remove_object
#   define e_cal_remove_object_with_mod EDSAbiWrapperSingleton.e_cal_remove_object_with_mod
#   define e_cal_set_auth_func EDSAbiWrapperSingleton.e_cal_set_auth_func
#   define icalcomponent_add_component EDSAbiWrapperSingleton.icalcomponent_add_component
#   define icalcomponent_as_ical_string EDSAbiWrapperSingleton.icalcomponent_as_ical_string
#   define icalcomponent_free EDSAbiWrapperSingleton.icalcomponent_free
#   define icalcomponent_get_first_component EDSAbiWrapperSingleton.icalcomponent_get_first_component
#   define icalcomponent_get_first_property EDSAbiWrapperSingleton.icalcomponent_get_first_property
#   define icalcomponent_get_next_component EDSAbiWrapperSingleton.icalcomponent_get_next_component
#   define icalcomponent_get_next_property EDSAbiWrapperSingleton.icalcomponent_get_next_property
#   define icalcomponent_get_recurrenceid EDSAbiWrapperSingleton.icalcomponent_get_recurrenceid
#   define icalcomponent_get_timezone EDSAbiWrapperSingleton.icalcomponent_get_timezone
#   define icalcomponent_get_uid EDSAbiWrapperSingleton.icalcomponent_get_uid
#   define icalcomponent_isa EDSAbiWrapperSingleton.icalcomponent_isa
#   define icalcomponent_new_clone EDSAbiWrapperSingleton.icalcomponent_new_clone
#   define icalcomponent_new_from_string EDSAbiWrapperSingleton.icalcomponent_new_from_string
#   define icalcomponent_remove_property EDSAbiWrapperSingleton.icalcomponent_remove_property
#   define icalcomponent_set_uid EDSAbiWrapperSingleton.icalcomponent_set_uid
#   define icalcomponent_vanew EDSAbiWrapperSingleton.icalcomponent_vanew
#   define icalparameter_get_tzid EDSAbiWrapperSingleton.icalparameter_get_tzid
#   define icalparameter_set_tzid EDSAbiWrapperSingleton.icalparameter_set_tzid
#   define icalproperty_get_description EDSAbiWrapperSingleton.icalproperty_get_description
#   define icalproperty_get_first_parameter EDSAbiWrapperSingleton.icalproperty_get_first_parameter
#   define icalproperty_get_lastmodified EDSAbiWrapperSingleton.icalproperty_get_lastmodified
#   define icalproperty_get_next_parameter EDSAbiWrapperSingleton.icalproperty_get_next_parameter
#   define icalproperty_get_summary EDSAbiWrapperSingleton.icalproperty_get_summary
#   define icalproperty_new_description EDSAbiWrapperSingleton.icalproperty_new_description
#   define icalproperty_new_summary EDSAbiWrapperSingleton.icalproperty_new_summary
#   define icalproperty_set_value_from_string EDSAbiWrapperSingleton.icalproperty_set_value_from_string
#   define icaltime_as_ical_string EDSAbiWrapperSingleton.icaltime_as_ical_string
#   define icaltimezone_free EDSAbiWrapperSingleton.icaltimezone_free
#   define icaltimezone_get_builtin_timezone EDSAbiWrapperSingleton.icaltimezone_get_builtin_timezone
#   define icaltimezone_get_builtin_timezone_from_tzid EDSAbiWrapperSingleton.icaltimezone_get_builtin_timezone_from_tzid
#   define icaltimezone_get_component EDSAbiWrapperSingleton.icaltimezone_get_component
#   define icaltimezone_get_tzid EDSAbiWrapperSingleton.icaltimezone_get_tzid
#   define icaltimezone_new EDSAbiWrapperSingleton.icaltimezone_new
#   define icaltimezone_set_component EDSAbiWrapperSingleton.icaltimezone_set_component
#  endif /* ENABLE_ECAL */
# endif /* EDS_ABI_WRAPPER_NO_REDEFINE */

#endif /* EVOLUTION_COMPATIBILITY */

/** initialize pointers to EDS functions, if necessary; can be called multiple times */
void EDSAbiWrapperInit();

const char *EDSAbiWrapperInfo();
const char *EDSAbiWrapperDebug();

#ifdef __cplusplus
}
#endif

#endif /* INCL_EDS_ABI_WRAPPER */