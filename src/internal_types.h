/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2010 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#ifndef _INTERNAL_TYPES_H
#define _INTERNAL_TYPES_H

#include <stdint.h>

#include "libforestdb/fdb_types.h"
#include "common.h"
#include "atomic.h"
#include "avltree.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hbtrie;
struct btree;
struct filemgr;
struct btreeblk_handle;
struct docio_handle;
struct btree_blk_ops;
struct snap_handle;

#define OFFSET_SIZE (sizeof(uint64_t))

#define FDB_MAX_KEYLEN_INTERNAL (65520)

/**
 * Error logging callback struct definition.
 */
struct err_log_callback {
    err_log_callback()
        : callback(nullptr)
        , callback_ex(nullptr)
        , ctx_data(nullptr) {}

    /**
     * Error logging callback function.
     */
    fdb_log_callback callback;
    /**
     * Extended error logging callback function.
     */
    fdb_log_callback_ex callback_ex;
    /**
     * Application-specific context data that is passed to the logging callback
     * function.
     */
    void *ctx_data;
};

typedef struct _fdb_transaction fdb_txn;

typedef uint64_t fdb_kvs_id_t;

typedef uint8_t kvs_type_t;
enum {
    KVS_ROOT = 0,
    KVS_SUB = 1
};

struct list;
struct kvs_opened_node;

/**
 * KV store info for each handle.
 */
struct kvs_info {
    /**
     * KV store type.
     */
    kvs_type_t type;
    /**
     * KV store ID.
     */
    fdb_kvs_id_t id;
    /**
     * Pointer to root handle.
     */
    fdb_kvs_handle *root;
};

/**
 * Attributes in KV store statistics.
 */
typedef enum {
    KVS_STAT_NLIVENODES,
    KVS_STAT_NDOCS,
    KVS_STAT_NDELETES,
    KVS_STAT_DATASIZE,
    KVS_STAT_WAL_NDOCS,
    KVS_STAT_WAL_NDELETES,
    KVS_STAT_DELTASIZE
} kvs_stat_attr_t;

/**
 * KV store statistics.
 */
struct kvs_stat {
    /**
     * The number of live index nodes.
     */
    uint64_t nlivenodes;
    /**
     * The number of documents.
     */
    uint64_t ndocs;
    /**
     * The number of deleted documents in main index.
     */
    uint64_t ndeletes;
    /**
     * The amount of space occupied by documents.
     */
    uint64_t datasize;
    /**
     * The number of documents in WAL.
     */
    uint64_t wal_ndocs;
    /**
     * The number of deleted documents in WAL.
     */
    uint64_t wal_ndeletes;
    /**
     * The amount of space occupied by documents+index since last commit.
     */
    int64_t deltasize;
};

// Versioning information...
// Version 002 - added stale-block tree info
#define FILEMGR_MAGIC_002 (UINT64_C(0xdeadcafebeefc002))
// Version 001 - added delta size to DB header and CRC-32C
#define FILEMGR_MAGIC_001 (UINT64_C(0xdeadcafebeefc001))
// Version 000 - old format (It involves various DB header formats so that we cannot
//               identify those different formats by using magic number. To avoid
//               unexpected behavior or crash, this magic number is no longer
//               supported.)
#define FILEMGR_MAGIC_000 (UINT64_C(0xdeadcafebeefbeef))
#define FILEMGR_LATEST_MAGIC FILEMGR_MAGIC_002

/**
 * Atomic counters of operational statistics in ForestDB KV store.
 */
struct kvs_ops_stat {

    kvs_ops_stat& operator=(const kvs_ops_stat& ops_stat) {
        atomic_store_uint64_t(&num_sets,
                              atomic_get_uint64_t(&ops_stat.num_sets,
                                                  std::memory_order_relaxed),
                              std::memory_order_relaxed);
        atomic_store_uint64_t(&num_dels,
                              atomic_get_uint64_t(&ops_stat.num_dels,
                                                  std::memory_order_relaxed),
                              std::memory_order_relaxed);
        atomic_store_uint64_t(&num_commits,
                              atomic_get_uint64_t(&ops_stat.num_commits,
                                                  std::memory_order_relaxed),
                              std::memory_order_relaxed);
        atomic_store_uint64_t(&num_compacts,
                              atomic_get_uint64_t(&ops_stat.num_compacts,
                                                  std::memory_order_relaxed),
                              std::memory_order_relaxed);
        atomic_store_uint64_t(&num_gets,
                              atomic_get_uint64_t(&ops_stat.num_gets,
                                                  std::memory_order_relaxed),
                              std::memory_order_relaxed);
        atomic_store_uint64_t(&num_iterator_gets,
                              atomic_get_uint64_t(&ops_stat.num_iterator_gets,
                                                  std::memory_order_relaxed),
                              std::memory_order_relaxed);
        atomic_store_uint64_t(&num_iterator_moves,
                              atomic_get_uint64_t(&ops_stat.num_iterator_moves,
                                                  std::memory_order_relaxed),
                              std::memory_order_relaxed);
        return *this;
    }

    // TODO: Move these variables to private members as we refactor the code in C++.
    /**
     * Number of fdb_set operations.
     */
    atomic_uint64_t num_sets;
    /**
     * Number of fdb_del operations.
     */
    atomic_uint64_t num_dels;
    /**
     * Number of fdb_commit operations.
     */
    atomic_uint64_t num_commits;
    /**
     * Number of fdb_compact operations on underlying file.
     */
    atomic_uint64_t num_compacts;
    /**
     * Number of fdb_get* (includes metaonly, byseq etc) operations.
     */
    atomic_uint64_t num_gets;
    /**
     * Number of fdb_iterator_get* (includes meta_only) operations.
     */
    atomic_uint64_t num_iterator_gets;
    /**
     * Number of fdb_iterator_moves (includes next,prev,seek) operations.
     */
    atomic_uint64_t num_iterator_moves;
};

#define FHANDLE_ROOT_OPENED (0x1)
#define FHANDLE_ROOT_INITIALIZED (0x2)
#define FHANDLE_ROOT_CUSTOM_CMP (0x4)

/**
 * ForestDB file handle definition.
 */
struct _fdb_file_handle {
    /**
     * The root KV store handle.
     */
    fdb_kvs_handle *root;
    /**
     * List of opened default KV store handles
     * (except for the root handle).
     */
    struct list *handles;
    /**
     * List of custom compare functions assigned by user
     */
    struct list *cmp_func_list;
    /**
     * Flags for the file handle.
     */
    uint64_t flags;
    /**
     * Spin lock for the file handle.
     */
    spin_t lock;
};

/**
 * ForestDB KV store key comparison callback context
 */
struct _fdb_key_cmp_info {
    /**
     * ForestDB KV store level config.
     */
    fdb_kvs_config kvs_config;
    /**
     * KV store information.
     */
    struct kvs_info *kvs;
};

/**
 * ForestDB KV store handle definition.
 */
struct _fdb_kvs_handle {

    _fdb_kvs_handle& operator=(const _fdb_kvs_handle& kv_handle) {
        kvs_config = kv_handle.kvs_config;
        kvs = kv_handle.kvs;
        op_stats = kv_handle.op_stats;
        fhandle = kv_handle.fhandle;
        trie = kv_handle.trie;
        staletree = kv_handle.staletree;
        if (kv_handle.kvs) {
            seqtrie = kv_handle.seqtrie;
        } else {
            seqtree = kv_handle.seqtree;
        }
        file = kv_handle.file;
        dhandle = kv_handle.dhandle;
        bhandle = kv_handle.bhandle;
        btreeblkops = kv_handle.btreeblkops;
        fileops = kv_handle.fileops;
        config = kv_handle.config;
        log_callback = kv_handle.log_callback;
        atomic_store_uint64_t(&cur_header_revnum, kv_handle.cur_header_revnum);
        atomic_store_uint64_t(&last_hdr_bid, kv_handle.last_hdr_bid);
        last_wal_flush_hdr_bid = kv_handle.last_wal_flush_hdr_bid;
        kv_info_offset = kv_handle.kv_info_offset;
        shandle = kv_handle.shandle;
        seqnum = kv_handle.seqnum;
        max_seqnum = kv_handle.max_seqnum;
        filename = kv_handle.filename;
        txn = kv_handle.txn;
        atomic_store_uint8_t(&handle_busy,
                             atomic_get_uint8_t(&kv_handle.handle_busy));
        dirty_updates = kv_handle.dirty_updates;
        node = kv_handle.node;
        num_iterators = kv_handle.num_iterators;
        bottom_up_build_entries = kv_handle.bottom_up_build_entries;
        return *this;
    }

    // TODO: Move these variables to private members as we refactor the code in C++.

    /**
     * ForestDB KV store level config. (Please retain as first struct member)
     */
    fdb_kvs_config kvs_config;
    /**
     * KV store information. (Please retain as second struct member)
     */
    struct kvs_info *kvs;
    /**
     * Operational statistics for this kv store.
     */
    struct kvs_ops_stat *op_stats;
    /**
     * Pointer to the corresponding file handle.
     */
    fdb_file_handle *fhandle;
    /**
     * HB+-Tree Trie instance.
     */
    struct hbtrie *trie;
    /**
     * Stale block B+-Tree instance.
     * Maps from 'commit revision number' to 'stale block info' system document.
     */
    struct btree *staletree;
    /**
     * Sequence B+-Tree instance.
     */
    union {
        struct btree *seqtree; // single KV instance mode
        struct hbtrie *seqtrie; // multi KV instance mode
    };
    /**
     * File manager instance.
     */
    struct filemgr *file;
    /**
     * Doc IO handle instance.
     */
    struct docio_handle *dhandle;
    /**
     * B+-Tree handle instance.
     */
    struct btreeblk_handle *bhandle;
    /**
     * B+-Tree block operation handle.
     */
    struct btree_blk_ops *btreeblkops;
    /**
     * File manager IO operation handle.
     */
    struct filemgr_ops *fileops;
    /**
     * ForestDB file level config.
     */
    fdb_config config;
    /**
     * Error logging callback.
     */
    err_log_callback log_callback;
    /**
     * File header revision number.
     */
    atomic_uint64_t cur_header_revnum;
    /**
     * Header revision number of rollback point.
     */
    uint64_t rollback_revnum;
    /**
     * Last header's block ID.
     * Why Atomic?
     * reader thread in fdb_sync_db_header can update last_hdr_bid while
     * writer thread sharing same file handle can get_oldest_active_header()
     * as part of its sb_reclaim_reusable_blocks()
     */
    atomic_uint64_t last_hdr_bid;
    /**
     * Block ID of a header created with most recent WAL flush.
     */
    uint64_t last_wal_flush_hdr_bid;
    /**
     * File offset of a document containing KV instance info.
     */
    uint64_t kv_info_offset;
    /**
     * Snapshot Information.
     */
    struct snap_handle *shandle;
    /**
     * KV store's current sequence number.
     */
    fdb_seqnum_t seqnum;
    /**
     * KV store's max sequence number for snapshot or rollback.
     */
    fdb_seqnum_t max_seqnum;
    /**
     * Virtual filename (DB instance filename given by users).
     */
    char *filename;
    /**
     * Transaction handle.
     */
    fdb_txn *txn;
    /**
     * Atomic flag to detect if handles are being shared among threads.
     */
    atomic_uint8_t handle_busy;
    /**
     * Flag that indicates whether this handle made dirty updates or not.
     */
    uint8_t dirty_updates;
    /**
     * List element that will be inserted into 'handles' list in the root handle.
     */
    struct kvs_opened_node *node;
    /**
     * Number of active iterator instances created from this handle
     */
    uint32_t num_iterators;
    /**
     * Used when `bottom_up_index_build` is `true`.
     * Instead of inserting into the WAL, it keeps <key, seqnum, offset> tuples
     * to build the index.
     */
    struct list* bottom_up_build_entries;
    /**
     * The number of entries in `bottom_up_build_entries`.
     */
    uint64_t num_bottom_up_build_entries;
    /**
     * The amount of data appended in bottom-up mode.
     */
    uint64_t space_used_for_bottom_up_build;
};

struct bottom_up_build_entry {
    struct list_elem le;
    void* key;
    size_t keylen;
    uint64_t offset;
    uint64_t seqnum;
};

struct hbtrie_iterator;
struct avl_tree;
struct avl_node;

/**
 * ForestDB iterator cursor movement direction
 */
typedef uint8_t fdb_iterator_dir_t;
enum {
    /**
     * Iterator cursor default.
     */
    FDB_ITR_DIR_NONE = 0x00,
    /**
     * Iterator cursor moving forward
     */
    FDB_ITR_FORWARD = 0x01,
    /**
     * Iterator cursor moving backwards
     */
    FDB_ITR_REVERSE = 0x02
};

/**
 * ForestDB iterator status
 */
typedef uint8_t fdb_iterator_status_t;
enum {
    /**
     * The last returned doc was retrieved from the main index.
     */
    FDB_ITR_IDX = 0x00,
    /**
     * The last returned doc was retrieved from the WAL.
     */
    FDB_ITR_WAL = 0x01
};

/**
 * ForestDB iterator structure definition.
 */
struct _fdb_iterator {
    /**
     * ForestDB KV store handle.
     */
    fdb_kvs_handle *handle;
    /**
     * HB+Trie iterator instance.
     */
    struct hbtrie_iterator *hbtrie_iterator;
    /**
     * B+Tree iterator for sequence number iteration
     */
    struct btree_iterator *seqtree_iterator;
    /**
     * HB+Trie iterator for sequence number iteration
     * (for multiple KV instance mode)
     */
    struct hbtrie_iterator *seqtrie_iterator;
    /**
     * Current seqnum pointed by the iterator.
     */
    fdb_seqnum_t _seqnum;
    /**
     * WAL Iterator to iterate over the shared sharded global WAL
     */
    struct wal_iterator *wal_itr;
    /**
     * Cursor instance of WAL iterator.
     */
    struct wal_item *tree_cursor;
    /**
     * Unique starting AVL node indicating the WAL iterator's start node.
     */
    struct wal_item *tree_cursor_start;
    /**
     * Previous position of WAL cursor.
     */
    struct wal_item *tree_cursor_prev;
    /**
     * Iterator start key.
     */
    void *start_key;
    union {
        /**
         * Iterator start seqnum.
         */
        fdb_seqnum_t start_seqnum;
        /**
         * Start key length.
         */
        size_t start_keylen;
    };
    /**
     * Iterator end key.
     */
    void *end_key;
    union {
        /**
         * Iterator end seqnum.
         */
        fdb_seqnum_t end_seqnum;
        /**
         * End key length.
         */
        size_t end_keylen;
    };
    /**
     * Iterator option.
     */
    fdb_iterator_opt_t opt;
    /**
     * Iterator cursor direction status.
     */
    fdb_iterator_dir_t direction;
    /**
     * The last returned document info.
     */
    fdb_iterator_status_t status;
    /**
     * Was this iterator created on an pre-existing snapshot handle
     */
    bool snapshot_handle;
    /**
     * Current key pointed by the iterator.
     */
    void *_key;
    /**
     * Length of key pointed by the iterator.
     */
    size_t _keylen;
    /**
     * Key offset.
     */
    uint64_t _offset;
    /**
     * Doc IO handle instance to the correct file.
     */
    struct docio_handle *_dhandle;
    /**
     * Cursor offset to key, meta and value on disk
     */
    uint64_t _get_offset;
};

struct wal_txn_wrapper;

/**
 * ForestDB transaction structure definition.
 */
struct _fdb_transaction {
    /**
     * ForestDB KV store handle.
     */
    fdb_kvs_handle *handle;
    /**
     * Unique monotonically increasing transaction id to distinguish
     * items that once belonged to a transaction which has ended.
     */
    uint64_t txn_id;
    /**
     * Block ID of the last header before the transaction begins.
     */
    uint64_t prev_hdr_bid;
    /**
     * Rev number of the last header before the transaction begins.
     */
    uint64_t prev_revnum;
    /**
     * List of dirty WAL items.
     */
    struct list *items;
    /**
     * Transaction isolation level.
     */
    fdb_isolation_level_t isolation;
    /**
     * Pointer to transaction wrapper.
     */
    struct wal_txn_wrapper *wrapper;
};

/* Global KV store header for each file
 */
struct kvs_header {
    /**
     * Monotonically increasing counter to generate KV store IDs.
     */
    fdb_kvs_id_t id_counter;
    /**
     * The custom comparison function if set by user.
     */
    fdb_custom_cmp_variable default_kvs_cmp;
    /**
     * Parameters for custom cmp function given by user.
     */
    void* default_kvs_cmp_param;
    /**
     * A tree linking all KV stores in a file by their KV store name.
     */
    struct avl_tree *idx_name;
    /**
     * A tree linking all KV stores in file by their ID.
     */
    struct avl_tree *idx_id;
    /**
     * Boolean to determine if custom compare function for a KV store is set.
     */
    uint8_t custom_cmp_enabled;
    /**
     * Number of KV store instances
     */
    size_t num_kv_stores;
    /**
     * lock to protect access to the idx_name and idx_id trees above
     */
    spin_t lock;
};

/** Mapping data for each KV store in DB file.
 * (global & most fields are persisted in the DB file)
 */
#define KVS_FLAG_CUSTOM_CMP (0x1)
struct kvs_node {
    /**
     * Name of the KV store as given by user.
     */
    char *kvs_name;
    /**
     * Unique KV Store ID generated and permanently assigned.
     */
    fdb_kvs_id_t id;
    /**
     * Highest sequence number seen in this KV store.
     */
    fdb_seqnum_t seqnum;
    /**
     * Flags indicating various states of the KV store.
     */
    uint64_t flags;
    /**
     * Custom compare function set by user (in-memory only).
     */
    fdb_custom_cmp_variable custom_cmp;
    /**
     * Parameter for custom cmp function given by user (in-memory only).
     */
    void* user_param;
    /**
     * Operational CRUD statistics for this KV store (in-memory only).
     */
    struct kvs_ops_stat op_stat;
    /**
     * Persisted KV store statistics.
     */
    struct kvs_stat stat;
    /**
     * Link to the global list of KV stores indexed by store name.
     */
    struct avl_node avl_name;
    /**
     * Link to the global list of KV stores indexed by store ID.
     */
    struct avl_node avl_id;
};

/**
 * Type of filename in use.
 */
typedef enum {
    /**
     * Filename used is a virtual filename (typically in auto compaction).
     */
    FDB_VFILENAME = 0,
    /**
     * Filename used is the actual filename (typically in manual compaction).
     */
    FDB_AFILENAME = 1,
} fdb_filename_mode_t;

/**
 * Stale data position & length
 */
struct stale_data {
    /**
     * Starting offset of the stale data
     */
    uint64_t pos;
    /**
     * Length of the stale data
     */
    uint32_t len;
    union {
        struct list_elem le;
        struct avl_node avl;
    };
};

/**
 * List of stale data
 */
struct stale_regions {
    /**
     * Number of regions
     */
    size_t n_regions;
    union {
        /**
         * Pointer to the array of regions, if n_regions > 1
         */
        struct stale_data *regions;
        /**
         * Stale region, if n_regions == 1
         */
        struct stale_data region;
    };
};

#define FDB_FLAG_SEQTREE_USE (0x1)
#define FDB_FLAG_ROOT_INITIALIZED (0x2)
#define FDB_FLAG_ROOT_CUSTOM_CMP (0x4)
#define FDB_FLAG_SUCCESSFULLY_COMPACTED (0x8)

#ifdef __cplusplus
}
#endif

#endif
