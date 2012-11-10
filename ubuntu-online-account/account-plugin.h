#ifndef __EMPATHY_ACCOUNTS_PLUGIN_WEBQQ_H__
#define __EMPATHY_ACCOUNTS_PLUGIN_WEBQQ_H__

#include "empathy-accounts-plugin.h"

G_BEGIN_DECLS

typedef struct _EmpathyAccountsPluginWebqq EmpathyAccountsPluginWebqq;
typedef struct _EmpathyAccountsPluginWebqqClass EmpathyAccountsPluginWebqqClass;

struct _EmpathyAccountsPluginWebqqClass
{
  /*<private>*/
  EmpathyAccountsPluginClass parent_class;
};

struct _EmpathyAccountsPluginWebqq
{
  /*<private>*/
  EmpathyAccountsPlugin parent;
};

GType empathy_accounts_plugin__get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_ACCOUNTS_PLUGIN_WEBQQ \
  (empathy_accounts_plugin_webqq_get_type ())
#define EMPATHY_ACCOUNTS_PLUGIN_WEBQQ(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN_WEBQQ, \
    EmpathyAccountsPluginWebqq))
#define EMPATHY_ACCOUNTS_PLUGIN_WEBQQ_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN_WEBQQ, \
    EmpathyAccountsPluginWebqqClass))
#define EMPATHY_IS_ACCOUNTS_PLUGIN_WEBQQ(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN_WEBQQ))
#define EMPATHY_IS_ACCOUNTS_PLUGIN_WEBQQ_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN_WEBQQ))
#define EMPATHY_ACCOUNTS_PLUGIN_WEBQQ_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
    EMPATHY_TYPE_ACCOUNTS_PLUGIN_WEBQQ, \
    EmpathyAccountsPluginWebqqClass))

GType ap_module_get_object_type (void);

G_END_DECLS

#endif /* #ifndef __EMPATHY_ACCOUNTS_PLUGIN_WEBQQ_H__*/
