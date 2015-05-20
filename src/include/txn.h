/*-
 * Copyright (c) 2014-2015 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#define	WT_TXN_NONE	0		/* No txn running in a session. */
#define	WT_TXN_FIRST	1		/* First transaction to run. */
#define	WT_TXN_ABORTED	UINT64_MAX	/* Update rolled back, ignore. */

/*
 * Transaction ID comparison dealing with edge cases.
 *
 * WT_TXN_ABORTED is the largest possible ID (never visible to a running
 * transaction), WT_TXN_NONE is smaller than any possible ID (visible to all
 * running transactions).
 */
#define	WT_TXNID_LE(t1, t2)						\
	((t1) <= (t2))

#define	WT_TXNID_LT(t1, t2)						\
	((t1) != (t2) && WT_TXNID_LE(t1, t2))

#define	WT_SESSION_TXN_STATE(s) (&S2C(s)->txn_global.states[(s)->id])

struct WT_COMPILER_TYPE_ALIGN(WT_CACHE_LINE_ALIGNMENT) __wt_txn_state {
	uint64_t id;
	uint64_t snap_min;
};

struct __wt_txn_global {
	uint64_t current;	/* Current transaction ID. */

	/* The oldest running transaction ID (may race). */
	uint64_t last_running;

	/*
	 * The oldest transaction ID that is not yet visible to some
	 * transaction in the system.
	 */
	uint64_t oldest_id;

	/* Count of scanning threads, or -1 for exclusive access. */
	int32_t scan_count;

	/*
	 * Track information about the running checkpoint. The transaction IDs
	 * used when checkpointing are special. Checkpoints can run for a long
	 * time so we keep them out of regular visibility checks. Eviction and
	 * checkpoint operations know when they need to be aware of
	 * checkpoint IDs.
	 */
	uint64_t checkpoint_gen;
	uint64_t checkpoint_id;
	uint64_t checkpoint_snap_min;

	WT_TXN_STATE *states;		/* Per-session transaction states */
};

typedef enum __wt_txn_isolation {
	WT_ISO_EVICTION,		/* Internal: eviction context */
	WT_ISO_READ_UNCOMMITTED,
	WT_ISO_READ_COMMITTED,
	WT_ISO_SNAPSHOT
} WT_TXN_ISOLATION;

/*
 * WT_TXN_OP --
 *	A transactional operation.  Each transaction builds an in-memory array
 *	of these operations as it runs, then uses the array to either write log
 *	records during commit or undo the operations during rollback.
 */
struct __wt_txn_op {
	uint32_t fileid;
	enum {
		WT_TXN_OP_BASIC,
		WT_TXN_OP_INMEM,
		WT_TXN_OP_REF,
		WT_TXN_OP_TRUNCATE_COL,
		WT_TXN_OP_TRUNCATE_ROW
	} type;
	union {
		/* WT_TXN_OP_BASIC, WT_TXN_OP_INMEM */
		WT_UPDATE *upd;
		/* WT_TXN_OP_REF */
		WT_REF *ref;
		/* WT_TXN_OP_TRUNCATE_COL */
		struct {
			uint64_t start, stop;
		} truncate_col;
		/* WT_TXN_OP_TRUNCATE_ROW */
		struct {
			WT_ITEM start, stop;
			enum {
				WT_TXN_TRUNC_ALL,
				WT_TXN_TRUNC_BOTH,
				WT_TXN_TRUNC_START,
				WT_TXN_TRUNC_STOP
			} mode;
		} truncate_row;
	} u;
};

/*
 * WT_TXN --
 *	Per-session transaction context.
 */
struct __wt_txn {
	uint64_t id;

	WT_TXN_ISOLATION isolation;

	/*
	 * Snapshot data:
	 *	ids < snap_min are visible,
	 *	ids > snap_max are invisible,
	 *	everything else is visible unless it is in the snapshot.
	 */
	uint64_t snap_min, snap_max;
	uint64_t *snapshot;
	uint32_t snapshot_count;
	uint32_t txn_logsync;	/* Log sync configuration */

	/* Array of modifications by this transaction. */
	WT_TXN_OP      *mod;
	size_t		mod_alloc;
	u_int		mod_count;

	/* Scratch buffer for in-memory log records. */
	WT_ITEM	       *logrec;

	/* Requested notification when transactions are resolved. */
	WT_TXN_NOTIFY *notify;

	/* Checkpoint status. */
	WT_LSN		ckpt_lsn;
	int		full_ckpt;
	uint32_t	ckpt_nsnapshot;
	WT_ITEM		*ckpt_snapshot;

#define	WT_TXN_AUTOCOMMIT	0x01
#define	WT_TXN_ERROR		0x02
#define	WT_TXN_HAS_ID	        0x04
#define	WT_TXN_HAS_SNAPSHOT	0x08
#define	WT_TXN_RUNNING		0x10
	uint32_t flags;
};
