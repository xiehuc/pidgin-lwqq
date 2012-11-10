#include "account-plugin.h"

G_DEFINE_TYPE (EmpathyAccountsPluginWebqq, empathy_accounts_plugin_webqq,\
        EMPATHY_TYPE_ACCOUNTS_PLUGIN)

static void
empathy_accounts_plugin_webqq_class_init (
    EmpathyAccountsPluginWebqqClass *klass)
{
}

static void
empathy_accounts_plugin_webqq_init (EmpathyAccountsPluginWebqq *self)
{
}

GType
ap_module_get_object_type (void)
{
  return EMPATHY_TYPE_ACCOUNTS_PLUGIN_WEBQQ;
}
