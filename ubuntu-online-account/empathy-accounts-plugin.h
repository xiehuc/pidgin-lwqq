/*
 * empathy-accounts-plugin.h
 *
 * Copyright (C) 2012 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __EMPATHY_ACCOUNTS_PLUGIN_H__
#define __EMPATHY_ACCOUNTS_PLUGIN_H__

#include <libaccount-plugin/plugin.h>

G_BEGIN_DECLS

typedef struct _EmpathyAccountsPlugin EmpathyAccountsPlugin;
typedef struct _EmpathyAccountsPluginClass EmpathyAccountsPluginClass;

struct _EmpathyAccountsPluginClass
{
  /*<private>*/
  ApPluginClass parent_class;
};

struct _EmpathyAccountsPlugin
{
  /*<private>*/
  ApPlugin parent;
};

GType empathy_accounts_plugin_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_ACCOUNTS_PLUGIN \
  (empathy_accounts_plugin_get_type ())
#define EMPATHY_ACCOUNTS_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN, \
    EmpathyAccountsPlugin))
#define EMPATHY_ACCOUNTS_PLUGIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN, \
    EmpathyAccountsPluginClass))
#define EMPATHY_IS_ACCOUNTS_PLUGIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN))
#define EMPATHY_IS_ACCOUNTS_PLUGIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN))
#define EMPATHY_ACCOUNTS_PLUGIN_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN, \
    EmpathyAccountsPluginClass))

G_END_DECLS

#endif /* #ifndef __EMPATHY_ACCOUNTS_PLUGIN_H__*/
