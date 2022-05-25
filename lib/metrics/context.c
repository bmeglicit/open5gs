#include "app/ogs-app.h"
#include "ogs-metrics.h"

#include "context.h"


int __ogs_metrics_domain;
static ogs_metrics_context_t self;
static int context_initialized = 0;


void ogs_metrics_context_init(void)
{
    ogs_assert(context_initialized == 0);

    /* Initialize metrics context */
    memset(&self, 0, sizeof(ogs_metrics_context_t));

    ogs_log_install_domain(&__ogs_metrics_domain, "metrics", ogs_core()->log.level);

    context_initialized = 1;
}

void ogs_metrics_context_final(void)
{
    ogs_freeaddrinfo(self.sockaddr);

    context_initialized = 0;
}

ogs_metrics_context_t *ogs_metrics_self(void)
{
    return &self;
}

int ogs_metrics_context_parse_config(void)
{
    yaml_document_t *document = NULL;
    ogs_yaml_iter_t root_iter;

    document = ogs_app()->document;
    ogs_assert(document);

    ogs_yaml_iter_init(&root_iter, document);
    while (ogs_yaml_iter_next(&root_iter)) {
        const char *root_key = ogs_yaml_iter_key(&root_iter);
        ogs_assert(root_key);

        if (!strcmp(root_key, "metrics")) {
            int family = AF_UNSPEC;
            const char *hostname = NULL;
            uint16_t port = OGS_METRICS_DEFAULT_HTTP_PORT;
            ogs_sockaddr_t *addr = NULL;
            int update_interval = OGS_METRICS_DEFAULT_UPDATE_INTERVAL;

            ogs_yaml_iter_t local_iter;
            ogs_yaml_iter_recurse(&root_iter, &local_iter);
            while (ogs_yaml_iter_next(&local_iter)) {
                const char *local_key = ogs_yaml_iter_key(&local_iter);

                if (!strcmp(local_key, "addr")) {
                    const char *v = ogs_yaml_iter_value(&local_iter);
                    if (v)
                        hostname = v;
                }
                else if (!strcmp(local_key, "port")) {
                    const char *v = ogs_yaml_iter_value(&local_iter);
                    if (v)
                        port = atoi(v);
                }
                else if (!strcmp(local_key, "interval")) {
                    const char *v = ogs_yaml_iter_value(&local_iter);
                    if (v)
                        update_interval = atoi(v);
                }
            }

            if (hostname != NULL)
            {
                ogs_assert(OGS_OK ==
                    ogs_addaddrinfo(&addr, family, hostname, port, 0));
                ogs_assert(OGS_OK == ogs_copyaddrinfo(&self.sockaddr, addr));
                ogs_freeaddrinfo(addr);
            }

            self.update_interval = ogs_time_from_sec(update_interval);
        }
   }

    return 0;
}
