/* Copyright (c) 2010
 * The Regents of the University of Michigan
 * All Rights Reserved
 *
 * Permission is granted to use, copy and redistribute this software
 * for noncommercial education and research purposes, so long as no
 * fee is charged, and so long as the name of the University of Michigan
 * is not used in any advertising or publicity pertaining to the use
 * or distribution of this software without specific, written prior
 * authorization.  Permission to modify or otherwise create derivative
 * works of this software is not granted.
 *
 * This software is provided as is, without representation or warranty
 * of any kind either express or implied, including without limitation
 * the implied warranties of merchantability, fitness for a particular
 * purpose, or noninfringement.  The Regents of the University of
 * Michigan shall not be liable for any damages, including special,
 * indirect, incidental, or consequential damages, with respect to any
 * claim arising out of or in connection with the use of the software,
 * even if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include "nfs41_callback.h"

#include "nfs41_ops.h"
#include "daemon_debug.h"

#define CBXLVL 2 /* dprintf level for callback xdr logging */
#define CBX_ERR(msg) dprintf((CBXLVL), __FUNCTION__ ": failed at " msg "\n")


/* common types */
static bool_t common_stateid(XDR *xdr, stateid4 *stateid)
{
    return xdr_u_int32_t(xdr, &stateid->seqid)
        && xdr_opaque(xdr, (char*)stateid->other, NFS4_STATEID_OTHER);
}

static bool_t common_fh(XDR *xdr, nfs41_fh *fh)
{
    return xdr_u_int32_t(xdr, &fh->len)
        && fh->len <= NFS4_FHSIZE
        && xdr_opaque(xdr, (char*)fh->fh, fh->len);
}

static bool_t common_fsid(XDR *xdr, nfs41_fsid *fsid)
{
    return xdr_u_int64_t(xdr, &fsid->major)
        && xdr_u_int64_t(xdr, &fsid->minor);
}

/* OP_CB_LAYOUTRECALL */
static bool_t op_cb_layoutrecall_file(XDR *xdr, struct cb_recall_file *args)
{
    bool_t result;

    result = common_fh(xdr, &args->fh);
    if (!result) { CBX_ERR("layoutrecall_file.fh"); goto out; }

    result = xdr_u_int64_t(xdr, &args->offset);
    if (!result) { CBX_ERR("layoutrecall_file.offset"); goto out; }

    result = xdr_u_int64_t(xdr, &args->length);
    if (!result) { CBX_ERR("layoutrecall_file.length"); goto out; }

    result = common_stateid(xdr, &args->stateid);
    if (!result) { CBX_ERR("layoutrecall_file.stateid"); goto out; }
out:
    return result;
}

static bool_t op_cb_layoutrecall_fsid(XDR *xdr, union cb_recall_file_args *args)
{
    bool_t result;

    result = common_fsid(xdr, &args->fsid);
    if (!result) { CBX_ERR("layoutrecall_fsid.fsid"); goto out; }
out:
    return result;
}

static const struct xdr_discrim cb_layoutrecall_discrim[] = {
    { PNFS_RETURN_FILE,     (xdrproc_t)op_cb_layoutrecall_file },
    { PNFS_RETURN_FSID,     (xdrproc_t)op_cb_layoutrecall_fsid },
    { PNFS_RETURN_ALL,      (xdrproc_t)xdr_void },
    { 0,                    NULL_xdrproc_t }
};

static bool_t op_cb_layoutrecall_args(XDR *xdr, struct cb_layoutrecall_args *args)
{
    bool_t result;

    result = xdr_enum(xdr, (enum_t*)&args->type);
    if (!result) { CBX_ERR("layoutrecall_args.type"); goto out; }

    result = xdr_enum(xdr, (enum_t*)&args->iomode);
    if (!result) { CBX_ERR("layoutrecall_args.iomode"); goto out; }

    result = xdr_bool(xdr, &args->changed);
    if (!result) { CBX_ERR("layoutrecall_args.changed"); goto out; }

    result = xdr_union(xdr, (enum_t*)&args->recall.type,
        (char*)&args->recall.args, cb_layoutrecall_discrim, NULL_xdrproc_t);
    if (!result) { CBX_ERR("layoutrecall_args.recall"); goto out; }
out:
    return result;
}

static bool_t op_cb_layoutrecall_res(XDR *xdr, struct cb_layoutrecall_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("layoutrecall_res.status"); goto out; }
out:
    return result;
}


/* OP_CB_RECALL_SLOT */
static bool_t op_cb_recall_slot_args(XDR *xdr, struct cb_recall_slot_args *res)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("recall_slot.target_highest_slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_recall_slot_res(XDR *xdr, struct cb_recall_slot_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("recall_slot.status"); goto out; }
out:
    return result;
}


/* OP_CB_SEQUENCE */
static bool_t op_cb_sequence_ref(XDR *xdr, struct cb_sequence_ref *args)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &args->sequenceid);
    if (!result) { CBX_ERR("sequence_ref.sequenceid"); goto out; }

    result = xdr_u_int32_t(xdr, &args->slotid);
    if (!result) { CBX_ERR("sequence_ref.slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_sequence_ref_list(XDR *xdr, struct cb_sequence_ref_list *args)
{
    bool_t result;

    result = xdr_opaque(xdr, args->sessionid, NFS4_SESSIONID_SIZE);
    if (!result) { CBX_ERR("sequence_ref_list.sessionid"); goto out; }

    result = xdr_array(xdr, (char**)&args->calls, &args->call_count,
        64, sizeof(struct cb_sequence_ref), (xdrproc_t)op_cb_sequence_ref);
    if (!result) { CBX_ERR("sequence_ref_list.calls"); goto out; }
out:
    return result;
}

static bool_t op_cb_sequence_args(XDR *xdr, struct cb_sequence_args *args)
{
    bool_t result;

    dprintf(1, "decoding sequence args\n");
    result = xdr_opaque(xdr, args->sessionid, NFS4_SESSIONID_SIZE);
    if (!result) { CBX_ERR("sequence_args.sessionid"); goto out; }

    result = xdr_u_int32_t(xdr, &args->sequenceid);
    if (!result) { CBX_ERR("sequence_args.sequenceid"); goto out; }

    result = xdr_u_int32_t(xdr, &args->slotid);
    if (!result) { CBX_ERR("sequence_args.slotid"); goto out; }

    result = xdr_u_int32_t(xdr, &args->highest_slotid);
    if (!result) { CBX_ERR("sequence_args.highest_slotid"); goto out; }

    result = xdr_bool(xdr, &args->cachethis);
    if (!result) { CBX_ERR("sequence_args.cachethis"); goto out; }

    result = xdr_array(xdr, (char**)&args->ref_lists,
        &args->ref_list_count, 64, sizeof(struct cb_sequence_ref_list),
        (xdrproc_t)op_cb_sequence_ref_list);
    if (!result) { CBX_ERR("sequence_args.ref_lists"); goto out; }
out:
    return result;
}

static bool_t op_cb_sequence_res_ok(XDR *xdr, struct cb_sequence_res_ok *res)
{
    bool_t result;

    result = xdr_opaque(xdr, res->sessionid, NFS4_SESSIONID_SIZE);
    if (!result) { CBX_ERR("sequence_res.sessionid"); goto out; }

    result = xdr_u_int32_t(xdr, &res->sequenceid);
    if (!result) { CBX_ERR("sequence_res.sequenceid"); goto out; }

    result = xdr_u_int32_t(xdr, &res->slotid);
    if (!result) { CBX_ERR("sequence_res.slotid"); goto out; }

    result = xdr_u_int32_t(xdr, &res->highest_slotid);
    if (!result) { CBX_ERR("sequence_res.highest_slotid"); goto out; }

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("sequence_res.target_highest_slotid"); goto out; }
out:
    return result;
}

static const struct xdr_discrim cb_sequence_res_discrim[] = {
    { NFS4_OK,              (xdrproc_t)op_cb_sequence_res_ok },
    { 0,                    NULL_xdrproc_t }
};

static bool_t op_cb_sequence_res(XDR *xdr, struct cb_sequence_res *res)
{
    bool_t result;

    result = xdr_union(xdr, &res->status, (char*)&res->ok,
        cb_sequence_res_discrim, (xdrproc_t)xdr_void);
    if (!result) { CBX_ERR("seq:argop.args"); goto out; }
out:
    return result;
}

/* OP_CB_GETATTR */
static bool_t op_cb_getattr_args(XDR *xdr, struct cb_getattr_args *res)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("getattr.target_highest_slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_getattr_res(XDR *xdr, struct cb_getattr_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("getattr.status"); goto out; }
out:
    return result;
}

/* OP_CB_RECALL */
static bool_t op_cb_recall_args(XDR *xdr, struct cb_recall_args *args)
{
    bool_t result;

    dprintf(1, "decoding recall args\n");
    result = common_stateid(xdr, &args->stateid);
    if (!result) { CBX_ERR("recall.stateid"); goto out; }

    result = xdr_bool(xdr, &args->truncate);
    if (!result) { CBX_ERR("recall.truncate"); goto out; }

    result = common_fh(xdr, &args->fh);
    if (!result) { CBX_ERR("recall.fh"); goto out; }
out:
    return result;
}

static bool_t op_cb_recall_res(XDR *xdr, struct cb_recall_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("recall.status"); goto out; }
out:
    return result;
}

/* OP_CB_NOTIFY */
static bool_t op_cb_notify_args(XDR *xdr, struct cb_notify_args *res)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("notify.target_highest_slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_notify_res(XDR *xdr, struct cb_notify_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("notify.status"); goto out; }
out:
    return result;
}

/* OP_CB_PUSH_DELEG */
static bool_t op_cb_push_deleg_args(XDR *xdr, struct cb_push_deleg_args *res)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("push_deleg.target_highest_slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_push_deleg_res(XDR *xdr, struct cb_push_deleg_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("push_deleg.status"); goto out; }
out:
    return result;
}

/* OP_CB_RECALL_ANY */
static bool_t op_cb_recall_any_args(XDR *xdr, struct cb_recall_any_args *res)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("recall_any.target_highest_slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_recall_any_res(XDR *xdr, struct cb_recall_any_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("recall_any.status"); goto out; }
out:
    return result;
}

/* OP_CB_RECALLABLE_OBJ_AVAIL */
static bool_t op_cb_recallable_obj_avail_args(XDR *xdr, struct cb_recallable_obj_avail_args *res)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("recallable_obj_avail.target_highest_slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_recallable_obj_avail_res(XDR *xdr, struct cb_recallable_obj_avail_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("recallable_obj_avail.status"); goto out; }
out:
    return result;
}

/* OP_CB_WANTS_CANCELLED */
static bool_t op_cb_wants_cancelled_args(XDR *xdr, struct cb_wants_cancelled_args *res)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("wants_cancelled.target_highest_slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_wants_cancelled_res(XDR *xdr, struct cb_wants_cancelled_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("wants_cancelled.status"); goto out; }
out:
    return result;
}

/* OP_CB_NOTIFY_LOCK */
static bool_t op_cb_notify_lock_args(XDR *xdr, struct cb_notify_lock_args *res)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("notify_lock.target_highest_slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_notify_lock_res(XDR *xdr, struct cb_notify_lock_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("notify_lock.status"); goto out; }
out:
    return result;
}

/* OP_CB_NOTIFY_DEVICEID */
static bool_t op_cb_notify_deviceid_args(XDR *xdr, struct cb_notify_deviceid_args *res)
{
    bool_t result;

    result = xdr_u_int32_t(xdr, &res->target_highest_slotid);
    if (!result) { CBX_ERR("notify_deviceid.target_highest_slotid"); goto out; }
out:
    return result;
}

static bool_t op_cb_notify_deviceid_res(XDR *xdr, struct cb_notify_deviceid_res *res)
{
    bool_t result;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("notify_deviceid.status"); goto out; }
out:
    return result;
}

/* CB_COMPOUND */
static bool_t cb_compound_tag(XDR *xdr, struct cb_compound_tag *args)
{
    return xdr_u_int32_t(xdr, &args->len)
        && args->len <= CB_COMPOUND_MAX_TAG
        && xdr_opaque(xdr, args->str, args->len);
}

static const struct xdr_discrim cb_argop_discrim[] = {
    { OP_CB_LAYOUTRECALL,   (xdrproc_t)op_cb_layoutrecall_args },
    { OP_CB_RECALL_SLOT,    (xdrproc_t)op_cb_recall_slot_args },
    { OP_CB_SEQUENCE,       (xdrproc_t)op_cb_sequence_args },
    { OP_CB_GETATTR,        (xdrproc_t)op_cb_getattr_args },
    { OP_CB_RECALL,         (xdrproc_t)op_cb_recall_args },
    { OP_CB_NOTIFY,         (xdrproc_t)op_cb_notify_args },
    { OP_CB_PUSH_DELEG,     (xdrproc_t)op_cb_push_deleg_args },
    { OP_CB_RECALL_ANY,     (xdrproc_t)op_cb_recall_any_args },
    { OP_CB_RECALLABLE_OBJ_AVAIL, (xdrproc_t)op_cb_recallable_obj_avail_args },
    { OP_CB_WANTS_CANCELLED, (xdrproc_t)op_cb_wants_cancelled_args },
    { OP_CB_NOTIFY_LOCK,     (xdrproc_t)op_cb_notify_lock_args },
    { OP_CB_NOTIFY_DEVICEID, (xdrproc_t)op_cb_notify_deviceid_args },
    { OP_CB_ILLEGAL,         NULL_xdrproc_t },
};

static bool_t cb_compound_argop(XDR *xdr, struct cb_argop *args)
{
    bool_t result;

    result = xdr_union(xdr, &args->opnum, (char*)&args->args,
        cb_argop_discrim, NULL_xdrproc_t);
    if (!result) { CBX_ERR("cmb:argop.args"); goto out; }
out:
    return result;
}

bool_t proc_cb_compound_args(XDR *xdr, struct cb_compound_args *args)
{
    bool_t result;

    result = cb_compound_tag(xdr, &args->tag);
    if (!result) { CBX_ERR("compound.tag"); goto out; }

    result = xdr_u_int32_t(xdr, &args->minorversion);
    if (!result) { CBX_ERR("compound.minorversion"); goto out; }

    /* "superfluous in NFSv4.1 and MUST be ignored by the client" */
    result = xdr_u_int32_t(xdr, &args->callback_ident);
    if (!result) { CBX_ERR("compound.callback_ident"); goto out; }

    result = xdr_array(xdr, (char**)&args->argarray,
        &args->argarray_count, CB_COMPOUND_MAX_OPERATIONS,
        sizeof(struct cb_argop), (xdrproc_t)cb_compound_argop);
    if (!result) { CBX_ERR("compound.argarray"); goto out; }
out:
    return result;
}

static const struct xdr_discrim cb_resop_discrim[] = {
    { OP_CB_LAYOUTRECALL,   (xdrproc_t)op_cb_layoutrecall_res },
    { OP_CB_RECALL_SLOT,    (xdrproc_t)op_cb_recall_slot_res },
    { OP_CB_SEQUENCE,       (xdrproc_t)op_cb_sequence_res },
    { OP_CB_GETATTR,        (xdrproc_t)op_cb_getattr_res },
    { OP_CB_RECALL,         (xdrproc_t)op_cb_recall_res },
    { OP_CB_NOTIFY,         (xdrproc_t)op_cb_notify_res },
    { OP_CB_PUSH_DELEG,     (xdrproc_t)op_cb_push_deleg_res },
    { OP_CB_RECALL_ANY,     (xdrproc_t)op_cb_recall_any_res },
    { OP_CB_RECALLABLE_OBJ_AVAIL, (xdrproc_t)op_cb_recallable_obj_avail_res },
    { OP_CB_WANTS_CANCELLED, (xdrproc_t)op_cb_wants_cancelled_res },
    { OP_CB_NOTIFY_LOCK,     (xdrproc_t)op_cb_notify_lock_res },
    { OP_CB_NOTIFY_DEVICEID, (xdrproc_t)op_cb_notify_deviceid_res },
    { OP_CB_ILLEGAL,         NULL_xdrproc_t },
};

static bool_t cb_compound_resop(XDR *xdr, struct cb_resop *res)
{
    bool_t result;

    result = xdr_union(xdr, &res->opnum, (char*)&res->res,
        cb_resop_discrim, NULL_xdrproc_t);
    if (!result) { CBX_ERR("resop.res"); goto out; }
out:
    return result;
}

bool_t proc_cb_compound_res(XDR *xdr, struct cb_compound_res *res)
{
    bool_t result;

    if (res == NULL)
        return TRUE;

    result = xdr_enum(xdr, &res->status);
    if (!result) { CBX_ERR("compound_res.status"); goto out; }

    result = cb_compound_tag(xdr, &res->tag);
    if (!result) { CBX_ERR("compound_res.tag"); goto out; }

    result = xdr_array(xdr, (char**)&res->resarray,
        &res->resarray_count, CB_COMPOUND_MAX_OPERATIONS,
        sizeof(struct cb_resop), (xdrproc_t)cb_compound_resop);
    if (!result) { CBX_ERR("compound_res.resarray"); goto out; }
out:
    free(res->resarray);
    free(res);

    return result;
}
