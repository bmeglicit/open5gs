/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "context.h"

#include "pfcp-path.h"
#include "n4-build.h"

static void pfcp_node_fsm_init(ogs_pfcp_node_t *node, bool try_to_assoicate)
{
    upf_event_t e;

    ogs_assert(node);

    memset(&e, 0, sizeof(e));
    e.pfcp_node = node;

    if (try_to_assoicate == true) {
        node->t_association = ogs_timer_add(ogs_app()->timer_mgr,
                upf_timer_association, node);
        ogs_assert(node->t_association);
    }

    ogs_fsm_init(&node->sm, upf_pfcp_state_initial, upf_pfcp_state_final, &e);
}

static void pfcp_node_fsm_fini(ogs_pfcp_node_t *node)
{
    upf_event_t e;

    ogs_assert(node);

    memset(&e, 0, sizeof(e));
    e.pfcp_node = node;

    ogs_fsm_fini(&node->sm, &e);

    if (node->t_association)
        ogs_timer_delete(node->t_association);
}

static void pfcp_recv_cb(short when, ogs_socket_t fd, void *data)
{
    int rv;

    ssize_t size;
    upf_event_t *e = NULL;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_sockaddr_t from;
    ogs_pfcp_node_t *node = NULL;
    ogs_pfcp_header_t *h = NULL;

    ogs_assert(fd != INVALID_SOCKET);

    pkbuf = ogs_pkbuf_alloc(NULL, OGS_MAX_SDU_LEN);
    ogs_assert(pkbuf);
    ogs_pkbuf_put(pkbuf, OGS_MAX_SDU_LEN);

    size = ogs_recvfrom(fd, pkbuf->data, pkbuf->len, 0, &from);
    if (size <= 0) {
        ogs_log_message(OGS_LOG_ERROR, ogs_socket_errno,
                "ogs_recvfrom() failed");
        ogs_pkbuf_free(pkbuf);
        return;
    }

    ogs_pkbuf_trim(pkbuf, size);

    h = (ogs_pfcp_header_t *)pkbuf->data;
    if (h->version != OGS_PFCP_VERSION) {
        ogs_pfcp_header_t rsp;

        ogs_error("Not supported version[%d]", h->version);

        memset(&rsp, 0, sizeof rsp);
        rsp.flags = (OGS_PFCP_VERSION << 5);
        rsp.type = OGS_PFCP_VERSION_NOT_SUPPORTED_RESPONSE_TYPE;
        rsp.length = htobe16(4);
        rsp.sqn_only = h->sqn_only;
        if (ogs_sendto(fd, &rsp, 8, 0, &from) < 0) {
            ogs_log_message(OGS_LOG_ERROR, ogs_socket_errno,
                    "ogs_sendto() failed");
        }
        ogs_pkbuf_free(pkbuf);

        return;
    }

    e = upf_event_new(UPF_EVT_N4_MESSAGE);
    ogs_assert(e);

    node = ogs_pfcp_node_find(&ogs_pfcp_self()->pfcp_peer_list, &from);
    if (!node) {
        node = ogs_pfcp_node_add(&ogs_pfcp_self()->pfcp_peer_list, &from);
        if (!node) {
            ogs_error("No memory: ogs_pfcp_node_add() failed");
            ogs_pkbuf_free(e->pkbuf);
            upf_event_free(e);
            return;
        }

        node->sock = data;
        pfcp_node_fsm_init(node, false);
    }
    e->pfcp_node = node;
    e->pkbuf = pkbuf;

    rv = ogs_queue_push(ogs_app()->queue, e);
    if (rv != OGS_OK) {
        ogs_error("ogs_queue_push() failed:%d", (int)rv);
        ogs_pkbuf_free(e->pkbuf);
        upf_event_free(e);
    }
}

int upf_pfcp_open(void)
{
    ogs_socknode_t *node = NULL;
    ogs_sock_t *sock = NULL;

    /* PFCP Server */
    ogs_list_for_each(&ogs_pfcp_self()->pfcp_list, node) {
        sock = ogs_pfcp_server(node);
        if (!sock) return OGS_ERROR;

        node->poll = ogs_pollset_add(ogs_app()->pollset,
                OGS_POLLIN, sock->fd, pfcp_recv_cb, sock);
        ogs_assert(node->poll);
    }
    ogs_list_for_each(&ogs_pfcp_self()->pfcp_list6, node) {
        sock = ogs_pfcp_server(node);
        if (!sock) return OGS_ERROR;

        node->poll = ogs_pollset_add(ogs_app()->pollset,
                OGS_POLLIN, sock->fd, pfcp_recv_cb, sock);
        ogs_assert(node->poll);
    }

    OGS_SETUP_PFCP_SERVER;

    return OGS_OK;
}

void upf_pfcp_close(void)
{
    ogs_pfcp_node_t *pfcp_node = NULL;

    ogs_list_for_each(&ogs_pfcp_self()->pfcp_peer_list, pfcp_node)
        pfcp_node_fsm_fini(pfcp_node);

    ogs_freeaddrinfo(ogs_pfcp_self()->pfcp_advertise);
    ogs_freeaddrinfo(ogs_pfcp_self()->pfcp_advertise6);

    ogs_socknode_remove_all(&ogs_pfcp_self()->pfcp_list);
    ogs_socknode_remove_all(&ogs_pfcp_self()->pfcp_list6);
}

int upf_pfcp_send_session_establishment_response(
        ogs_pfcp_xact_t *xact, upf_sess_t *sess,
        ogs_pfcp_pdr_t *created_pdr[], int num_of_created_pdr)
{
    int rv;
    ogs_pkbuf_t *n4buf = NULL;
    ogs_pfcp_header_t h;

    ogs_assert(xact);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_SESSION_ESTABLISHMENT_RESPONSE_TYPE;
    h.seid = sess->smf_n4_f_seid.seid;

    n4buf = upf_n4_build_session_establishment_response(
            h.type, sess, created_pdr, num_of_created_pdr);
    if (!n4buf) {
        ogs_error("upf_n4_build_session_establishment_response() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, n4buf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    return rv;
}

int upf_pfcp_send_session_modification_response(
        ogs_pfcp_xact_t *xact, upf_sess_t *sess,
        ogs_pfcp_pdr_t *created_pdr[], int num_of_created_pdr)
{
    int rv;
    ogs_pkbuf_t *n4buf = NULL;
    ogs_pfcp_header_t h;

    ogs_assert(xact);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_SESSION_MODIFICATION_RESPONSE_TYPE;
    h.seid = sess->smf_n4_f_seid.seid;

    n4buf = upf_n4_build_session_modification_response(
            h.type, sess, created_pdr, num_of_created_pdr);
    if (!n4buf) {
        ogs_error("upf_n4_build_session_modification_response() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, n4buf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    return rv;
}

int upf_pfcp_send_session_deletion_response(ogs_pfcp_xact_t *xact,
        upf_sess_t *sess)
{
    int rv;
    ogs_pkbuf_t *n4buf = NULL;
    ogs_pfcp_header_t h;

    ogs_assert(xact);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_SESSION_DELETION_RESPONSE_TYPE;
    h.seid = sess->smf_n4_f_seid.seid;

    n4buf = upf_n4_build_session_deletion_response(h.type, sess);
    if (!n4buf) {
        ogs_error("upf_n4_build_session_deletion_response() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, n4buf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    return rv;
}

static void sess_timeout(ogs_pfcp_xact_t *xact, void *data)
{
    upf_sess_t *sess = NULL;
    ogs_pool_id_t sess_id = OGS_INVALID_POOL_ID;
    uint8_t type;

    ogs_assert(xact);
    type = xact->seq[0].type;

    ogs_assert(data);
    sess_id = OGS_POINTER_TO_UINT(data);
    ogs_assert(sess_id >= OGS_MIN_POOL_ID && sess_id <= OGS_MAX_POOL_ID);

    sess = upf_sess_find_by_id(sess_id);
    if (!sess) {
        ogs_error("Session has already been removed [%d]", type);
        return;
    }

    switch (type) {
    case OGS_PFCP_SESSION_REPORT_REQUEST_TYPE:
        ogs_error("No PFCP session report response");
        break;
    default:
        ogs_error("Not implemented [type:%d]", type);
        break;
    }
}

int upf_pfcp_send_session_report_request(
        upf_sess_t *sess, ogs_pfcp_user_plane_report_t *report)
{
    int rv;
    ogs_pkbuf_t *n4buf = NULL;
    ogs_pfcp_header_t h;
    ogs_pfcp_xact_t *xact = NULL;

    upf_metrics_inst_global_inc(UPF_METR_GLOB_CTR_SM_N4SESSIONREPORT);

    ogs_assert(sess);
    ogs_assert(report);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_SESSION_REPORT_REQUEST_TYPE;
    h.seid = sess->smf_n4_f_seid.seid;

    xact = ogs_pfcp_xact_local_create(
            sess->pfcp_node, sess_timeout, OGS_UINT_TO_POINTER(sess->id));
    if (!xact) {
        ogs_error("ogs_pfcp_xact_local_create() failed");
        return OGS_ERROR;
    }

    n4buf = ogs_pfcp_build_session_report_request(h.type, report);
    if (!n4buf) {
        ogs_error("ogs_pfcp_build_session_report_request() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, n4buf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }


    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    return rv;
}
