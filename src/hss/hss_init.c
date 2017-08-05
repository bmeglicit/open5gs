#define TRACE_MODULE _hss_init

#include "hss_context.h"
#include "hss_fd.h"
#include "hss_s6a_handler.h"

static int initialized = 0;

status_t hss_initialize(void)
{
    status_t rv;

    rv = hss_context_init();
    if (rv != CORE_OK) return rv;

    rv = hss_context_parse_config();
    if (rv != CORE_OK) return rv;

    rv = hss_context_setup_trace_module();
    if (rv != CORE_OK) return rv;

    rv = hss_db_init();
    if (rv != CORE_OK) return rv;

    rv = hss_fd_init();
    if (rv != 0) return rv;

    rv = hss_s6a_init();
    if (rv != CORE_OK) return rv;

    initialized = 1;

	return CORE_OK;
}

void hss_terminate(void)
{
    if (!initialized) return;

    hss_s6a_final();
    hss_fd_final();

    hss_db_final();
    hss_context_final();
	
	return;
}
