/*
 * Copyright (C) 2009 Intel Corporation
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

#ifndef SYNC_UI_CONFIG_H
#define SYNC_UI_CONFIG_H

#include <gtk/gtk.h>
#include "syncevo-dbus.h"

typedef enum {
    SYNC_NONE,
    SYNC_TWO_WAY,
    SYNC_SLOW,
    SYNC_ONE_WAY_FROM_CLIENT,
    SYNC_REFRESH_FROM_CLIENT,
    SYNC_ONE_WAY_FROM_SERVER,
    SYNC_REFRESH_FROM_SERVER,
    SYNC_MODE_MAX
}SyncMode;

typedef struct source_config {
    char *name;
    gboolean enabled;
    gboolean supported_locally;
    char *uri;
} source_config;

typedef struct server_config {
    char *name;
    char *base_url;
    char *web_url;
    char *icon_uri;

    char *username;
    char *password;

    GList *source_configs;
    
    /* any field in struct has changed */
    gboolean changed;

    /* a authentication detail (base_url/username/password) has changed */
    gboolean auth_changed;

    gboolean password_changed;

    gboolean from_template;
} server_config;

void source_config_free (source_config *source);

void server_config_free (server_config *server);
void server_config_update_from_option (server_config *server, SyncevoOption *option);
void server_config_update_from_entry (server_config *server, GtkEntry *entry);
GPtrArray* server_config_get_option_array (server_config *server);
GPtrArray* server_config_get_source_array (server_config *server, SyncMode mode);
source_config* server_config_get_source_config (server_config *server, const char *name);

void server_config_ensure_default_sources_exist (server_config *server);
void add_server_option (SyncevoOption *option, server_config *server);


/* data structure for syncevo_service_get_template_config_async and
 * syncevo_service_get_server_config_async. server is the server that
 * the method was called for. options_override are options that should
 * be overridden on the config we get.
 */
typedef struct server_data {
    server_config *config;
    GPtrArray *options_override;
    gpointer *data;
} server_data;
server_data* server_data_new (const char *name, gpointer *data);
void server_data_free (server_data *data, gboolean free_config);

#endif
