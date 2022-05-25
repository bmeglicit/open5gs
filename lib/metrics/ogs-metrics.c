#include "ogs-app.h"
#include "ogs-metrics.h"

#include <microhttpd.h>


/**
 * @brief Convert metric type to string
 *
 */
static const char *metric_type_to_str(ogs_metrics_type_e type)
{
    switch (type)
    {
        case OGS_METRICS_TYPE_GAUGE:
            return "gauge";
        case OGS_METRICS_TYPE_COUNTER:
            return "counter";
        case OGS_METRICS_TYPE_HISTOGRAM:
            return "histogram";
        case OGS_METRICS_TYPE_SUMMARY:
            return "summary";
    }

    return "unknown";
}

/**
 * @brief Convert list of metrics into textual representation for export
 *
 */
static char *export_metrics(char *resp, ogs_list_t *metrics)
{
    ogs_metrics_metric_t *metric = NULL;

    ogs_list_for_each(metrics, metric)
    {
        resp = ogs_mstrcatf(resp, "# HELP %s %s\n# TYPE %s %s\n",
            metric->name, metric->help,
            metric->name, metric_type_to_str(metric->type));

        if (metric->sample.valid == true)
        {
            resp = ogs_mstrcatf(resp, "%s %zd\n",
                metric->name, metric->sample.val_gauge);
        }

        if (ogs_list_count(&metric->label_list) > 0)
        {
            ogs_metrics_label_list_t *label_list;
            ogs_list_for_each(&metric->label_list, label_list)
            {
                int cnt = 0;
                ogs_metrics_label_t *label;

                ogs_list_for_each(&label_list->labels, label)
                {
                    resp = ogs_mstrcatf(resp, "%s%s%s=\"%s\"",
                        (cnt == 0) ? metric->name : "",
                        (cnt > 0) ? "," : "",
                        label->label_name, label->label_value);

                    cnt++;
                }

                resp = ogs_mstrcatf(resp, "} %d\n",
                    (int)label_list->sample.val_gauge);
            }
        }
    }

    return resp;
}

/**
 * @brief microhttpd callback handler for processing HTTP requests
 *
 */
static enum MHD_Result req_handler(void *cls, struct MHD_Connection *connection,
        const char *url, const char *method, const char *version,
        const char *upload_data, size_t *upload_data_size, void **con_cls)
{
    ogs_metrics_context_t *self = ogs_metrics_self();
    unsigned int code = MHD_HTTP_BAD_REQUEST;
    struct MHD_Response *response;
    enum MHD_Result ret;

    /* clear the response from previous request */
    if (self->http_resp)
        ((char *)self->http_resp)[0] = '\0';

    if (strcmp(method, "GET") != 0)
    {
        self->http_resp = ogs_mstrcatf(self->http_resp, "Invalid HTTP Method\n");
        code = MHD_HTTP_BAD_REQUEST;
    }
    else if (!(strcmp(url, "/")))
    {
        self->http_resp = ogs_mstrcatf(self->http_resp, "OK\n");
        code = MHD_HTTP_OK;
    }
    else if (!(strcmp(url, "/metrics")))
    {
        self->http_resp = export_metrics(self->http_resp, &self->metrics);
        code = MHD_HTTP_OK;
    }
    else
    {
        self->http_resp = ogs_mstrcatf(self->http_resp, "Bad Request\n");
        code = MHD_HTTP_BAD_REQUEST;
    }

    response = MHD_create_response_from_buffer(
            strlen(self->http_resp), (void *)self->http_resp,
            MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, code, response);
    MHD_destroy_response(response);

    return ret;
}

/**
 * @brief Callback when update timer expires
 *
 */
static void metrics_update_cb(void *data)
{
    ogs_metrics_context_t *self = ogs_metrics_self();

    if (self->update_cb != NULL)
        self->update_cb();

    ogs_timer_start(self->update_timer, self->update_interval);
}

/**
 * @brief Callback used by microhttpd
 *
 */
static void run(short when, ogs_socket_t fd, void *data)
{
    struct MHD_Daemon *mhd_daemon = data;

    ogs_assert(mhd_daemon);
    MHD_run(mhd_daemon);
}

/**
 * @brief Callback called by microhttpd when status of connection changes
 *
 */
static void notify_connection(void *cls,
        struct MHD_Connection *connection,
        void **socket_context,
        enum MHD_ConnectionNotificationCode toe)
{
    struct MHD_Daemon *mhd_daemon = NULL;
    MHD_socket mhd_socket = INVALID_SOCKET;
    const union MHD_ConnectionInfo *mhd_info = NULL;
    ogs_poll_t *poll_read;

    switch (toe) {
        case MHD_CONNECTION_NOTIFY_STARTED:
            mhd_info = MHD_get_connection_info(
                    connection, MHD_CONNECTION_INFO_DAEMON);
            ogs_assert(mhd_info);
            mhd_daemon = mhd_info->daemon;
            ogs_assert(mhd_daemon);

            mhd_info = MHD_get_connection_info(
                    connection, MHD_CONNECTION_INFO_CONNECTION_FD);
            ogs_assert(mhd_info);
            mhd_socket = mhd_info->connect_fd;
            ogs_assert(mhd_socket != INVALID_SOCKET);

            poll_read = ogs_pollset_add(ogs_app()->pollset,
                    OGS_POLLIN, mhd_socket, run, mhd_daemon);
            ogs_assert(poll_read);
            *socket_context = poll_read;
            break;
        case MHD_CONNECTION_NOTIFY_CLOSED:
            poll_read = *socket_context;
            if (poll_read)
                ogs_pollset_remove(poll_read);
            break;
    }
}


int ogs_metrics_init(void)
{
    ogs_metrics_context_t *self = ogs_metrics_self();

    ogs_list_init(&self->metrics);

    return OGS_OK;
}

void ogs_metrics_deinit(void)
{
    ogs_metrics_context_t *self = ogs_metrics_self();
    ogs_metrics_metric_t *metric;
    ogs_metrics_metric_t *next_metric;

    ogs_list_for_each_safe(&self->metrics, next_metric, metric)
    {
        if (metric->name)
            ogs_free(metric->name);
        if (metric->help)
            ogs_free(metric->help);

        ogs_metrics_label_list_t *label_list;
        ogs_metrics_label_list_t *next_label_list;
        ogs_list_for_each_safe(&metric->label_list, next_label_list, label_list)
        {
            ogs_metrics_label_t *label;
            ogs_metrics_label_t *next_label;
            ogs_list_for_each_safe(&label_list->labels, next_label, label)
            {
                if (label->label_name)
                    ogs_free(label->label_name);
                if (label->label_value)
                    ogs_free(label->label_value);

                ogs_list_remove(&label_list->labels, label);
                ogs_free(label);
            }

            ogs_list_remove(&metric->label_list, label_list);
            ogs_free(label_list);
        }

        ogs_list_remove(&self->metrics, metric);
        ogs_free(metric);
    }

    ogs_timer_delete(self->update_timer);
}


int ogs_metrics_start(void)
{
    ogs_metrics_context_t *self = ogs_metrics_self();

    const union MHD_DaemonInfo *mhd_info = NULL;
#define MAX_NUM_OF_MHD_OPTION_ITEM 4
    struct MHD_OptionItem mhd_ops[MAX_NUM_OF_MHD_OPTION_ITEM];
    int index = 0;

    ogs_sockaddr_t *addr;
    char buf[OGS_ADDRSTRLEN];
    char *hostname;

    addr = self->sockaddr;
    if (addr == NULL)
        return OGS_OK;

    hostname = ogs_gethostname(addr);
    if (hostname)
        ogs_info("metrics_server() [%s]:%d", hostname, OGS_PORT(addr));
    else
        ogs_info("metrics_server() [%s]:%d", OGS_ADDR(addr, buf), OGS_PORT(addr));


    mhd_ops[index].option = MHD_OPTION_NOTIFY_CONNECTION;
    mhd_ops[index].value = (intptr_t)&notify_connection;
    mhd_ops[index].ptr_value = NULL;
    index++;

    mhd_ops[index].option = MHD_OPTION_SOCK_ADDR;
    mhd_ops[index].value = 0;
    mhd_ops[index].ptr_value = (void *)&addr->sa;
    index++;

    mhd_ops[index].option = MHD_OPTION_END;
    mhd_ops[index].value = 0;
    mhd_ops[index].ptr_value = NULL;

    self->mhd_daemon = MHD_start_daemon(
            MHD_ALLOW_SUSPEND_RESUME | MHD_USE_ERROR_LOG,
            0,
            NULL, NULL,
            &req_handler, NULL,
            MHD_OPTION_ARRAY, mhd_ops,
            MHD_OPTION_END);
    if (self->mhd_daemon == NULL)
    {
        ogs_error("Failed to start metrics server");
        return OGS_ERROR;
    }

    mhd_info = MHD_get_daemon_info(self->mhd_daemon, MHD_DAEMON_INFO_LISTEN_FD);
    self->node.poll = ogs_pollset_add(ogs_app()->pollset,
            OGS_POLLIN, mhd_info->listen_fd, run, self->mhd_daemon);

    self->update_timer = ogs_timer_add(ogs_app()->timer_mgr, metrics_update_cb, NULL);
    ogs_timer_start(self->update_timer, self->update_interval);

    return OGS_OK;
}

void ogs_metrics_close(void)
{
    ogs_metrics_context_t *self = ogs_metrics_self();

    if (self->node.poll)
        ogs_pollset_remove(self->node.poll);

    if (self->mhd_daemon != NULL) {
        MHD_stop_daemon(self->mhd_daemon);
        self->mhd_daemon = NULL;
    }
}

void ogs_metrics_set_update_cb(ogs_metrics_update_cb_t cb)
{
    ogs_metrics_context_t *self = ogs_metrics_self();

    self->update_cb = cb;
}

ogs_metrics_sample_t *ogs_metrics_create(ogs_metrics_type_e type,
        const char *name, const char *help,
        unsigned int label_cnt, const char *labels[][2])
{
    ogs_metrics_context_t *self = ogs_metrics_self();
    ogs_metrics_metric_t *metric;
    bool found_existing = false;
    ogs_metrics_sample_t *sample;
    int i;

    ogs_list_for_each(&self->metrics, metric)
    {
        if (!(strcmp(metric->name, name)) &&
            !(strcmp(metric->help, help)))
        {
            if (metric->type != type)
            {
                ogs_error("Found existing metric with a different type");
                return NULL;
            }

            found_existing = true;
            break;
        }
    }

    if (found_existing == false)
    {
        metric = ogs_malloc(sizeof(ogs_metrics_metric_t));
        metric->type = type;
        metric->name = ogs_strdup(name);
        metric->help = ogs_strdup(help);
        metric->sample.valid = false;

        ogs_list_init(&metric->label_list);
        ogs_list_add(&self->metrics, metric);
    }

    if (label_cnt > 0)
    {
        ogs_metrics_label_list_t *label_list;
        label_list = ogs_malloc(sizeof(ogs_metrics_label_list_t));
        ogs_list_add(&metric->label_list, label_list);
        ogs_list_init(&label_list->labels);

        for (i = 0; i < label_cnt; i++)
        {
            ogs_metrics_label_t *label;
            label = ogs_malloc(sizeof(ogs_metrics_label_t));

            label->label_name = ogs_strdup(labels[i][0]);
            label->label_value = ogs_strdup(labels[i][1]);

            ogs_list_add(&label_list->labels, label);
        }

        sample = &label_list->sample;
    }
    else
    {
        sample = &metric->sample;
    }

    return sample;
}

void ogs_metrics_gauge_set(ogs_metrics_sample_t *sample, ssize_t value)
{
    sample->valid = true;
    sample->val_gauge = value;
}

ssize_t ogs_metrics_gauge_get(ogs_metrics_sample_t *sample)
{
    return sample->val_gauge;
}
