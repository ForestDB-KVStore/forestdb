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

#ifndef _FDB_INTERNAL_H
#define _FDB_INTERNAL_H

#include "common.h"
#include "internal_types.h"
#include "avltree.h"
#include "btreeblock.h"
#include "hbtrie.h"
#include "docio.h"
#include "staleblock.h"
#include "log_message.h"

#include <functional>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* If non-NULL, callback invoked when handling a fatal error. */
extern fdb_fatal_error_callback fatal_error_callback;

void buf2kvid(size_t chunksize, void *buf, fdb_kvs_id_t *id);
void kvid2buf(size_t chunksize, fdb_kvs_id_t id, void *buf);
void buf2buf(size_t chunksize_src, void *buf_src,
             size_t chunksize_dst, void *buf_dst);

size_t _fdb_readkey_wrap(void *handle, uint64_t offset, void *buf);
size_t _fdb_readseq_wrap(void *handle, uint64_t offset, void *buf);
int _fdb_custom_cmp_wrap(void *key1, void *key2, void *aux);

fdb_status _fdb_clone_snapshot(fdb_kvs_handle *handle_in,
                               fdb_kvs_handle *handle_out);
fdb_status _fdb_open(fdb_kvs_handle *handle,
                     const char *filename,
                     fdb_filename_mode_t filename_mode,
                     const fdb_config *config);
fdb_status _fdb_close_root(fdb_kvs_handle *handle);
fdb_status _fdb_close(fdb_kvs_handle *handle);
fdb_status _fdb_commit(fdb_kvs_handle *handle,
                       fdb_commit_opt_t opt,
                       bool sync);

fdb_status fdb_check_file_reopen(fdb_kvs_handle *handle, file_status_t *status);
void fdb_sync_db_header(fdb_kvs_handle *handle);

void fdb_fetch_header(uint64_t version,
                      void *header_buf,
                      bid_t *trie_root_bid,
                      bid_t *seq_root_bid,
                      bid_t *stale_root_bid,
                      uint64_t *ndocs,
                      uint64_t *ndeletes,
                      uint64_t *nlivenodes,
                      uint64_t *datasize,
                      uint64_t *last_wal_flush_hdr_bid,
                      uint64_t *kv_info_offset,
                      uint64_t *header_flags,
                      char **new_filename,
                      char **old_filename);
uint64_t fdb_set_file_header(fdb_kvs_handle *handle, bool inc_revnum);

fdb_status fdb_open_for_compactor(fdb_file_handle **ptr_fhandle,
                                  const char *filename,
                                  fdb_config *fconfig,
                                  struct list *cmp_func_list);

fdb_status fdb_compact_file(fdb_file_handle *fhandle,
                            const char *new_filename,
                            bool in_place_compaction,
                            bid_t marker_bid,
                            bool clone_docs,
                            const fdb_encryption_key *new_encryption_key,
                            const fdb_compact_opt* opt);

fdb_status _fdb_abort_transaction(fdb_kvs_handle *handle);

typedef enum {
    FDB_RESTORE_NORMAL,
    FDB_RESTORE_KV_INS,
} fdb_restore_mode_t;

void fdb_file_handle_init(fdb_file_handle *fhandle,
                           fdb_kvs_handle *root);
void fdb_file_handle_close_all(fdb_file_handle *fhandle);
void fdb_file_handle_parse_cmp_func(fdb_file_handle *fhandle,
                                    size_t n_func,
                                    char **kvs_names,
                                    fdb_custom_cmp_variable *functions,
                                    void **user_params);
void fdb_file_handle_clone_cmp_func_list(fdb_file_handle *fhandle,
                                         struct list *cmp_func_list);
void fdb_file_handle_add_cmp_func(fdb_file_handle *fhandle,
                                  char *kvs_name,
                                  fdb_custom_cmp_variable cmp_func,
                                  void* cmp_func_param);
void fdb_file_handle_free(fdb_file_handle *fhandle);

void fdb_cmp_func_list_from_filemgr(struct filemgr *file,
                                    struct list *cmp_func_list);
void fdb_free_cmp_func_list(struct list *cmp_func_list);

fdb_status fdb_kvs_cmp_check(fdb_kvs_handle *handle);
void fdb_kvs_find_cmp_chunk(void *chunk,
                            void *aux,
                            hbtrie_cmp_func** cmp_func_out,
                            void** user_param_out);

void fdb_kvs_info_create(fdb_kvs_handle *root_handle,
                         fdb_kvs_handle *handle,
                         struct filemgr *file,
                         const char *kvs_name);
void fdb_kvs_info_free(fdb_kvs_handle *handle);
void fdb_kvs_header_reset_all_stats(struct filemgr *file);
void fdb_kvs_header_create(struct filemgr *file);
uint64_t fdb_kvs_header_append(fdb_kvs_handle *handle);

struct kvs_header;

void fdb_kvs_header_read(struct kvs_header *kv_header,
                         struct docio_handle *dhandle,
                         uint64_t kv_info_offset,
                         uint64_t version,
                         bool only_seq_nums);
void fdb_kvs_header_copy(fdb_kvs_handle *handle,
                         struct filemgr *new_file,
                         struct docio_handle *new_dhandle,
                         uint64_t *new_file_kv_info_offset,
                         bool create_new);
void _fdb_kvs_init_root(fdb_kvs_handle *handle, struct filemgr *file);
void _fdb_kvs_header_create(struct kvs_header **kv_header_ptr);
void _fdb_kvs_header_import(struct kvs_header *kv_header,
                            void *data, size_t len, uint64_t version,
                            bool only_seq_nums);

fdb_status _fdb_kvs_get_snap_info(void *data, uint64_t version,
                                  fdb_snapshot_info_t *snap_info);
void _fdb_kvs_header_free(struct kvs_header *kv_header);
fdb_seqnum_t _fdb_kvs_get_seqnum(struct kvs_header *kv_header,
                                 fdb_kvs_id_t id);
uint64_t _kvs_stat_get_sum_attr(void *data, uint64_t version,
                                kvs_stat_attr_t attr);

bool _fdb_kvs_is_busy(fdb_file_handle *fhandle);

void fdb_kvs_header_free(struct filemgr *file);

char* _fdb_kvs_get_name(fdb_kvs_handle *kv_ins, struct filemgr *file);
/**
 * Extracts the KV Store name from a key sample and offset to start of user key
 * @param handle - pointer to root handle
 * @param keybuf - pointer to key which may include the KV Store Id prefix
 * @param key_offset - return variable of offset to where real key begins
 */
const char* _fdb_kvs_extract_name_off(fdb_kvs_handle *handle, void *keybuf,
                                      size_t *key_offset);

fdb_status _fdb_kvs_clone_snapshot(fdb_kvs_handle *handle_in,
                                   fdb_kvs_handle *handle_out);
fdb_status _fdb_kvs_open(fdb_kvs_handle *root_handle,
                         fdb_config *config,
                         fdb_kvs_config *kvs_config,
                         struct filemgr *file,
                         const char *filename,
                         const char *kvs_name,
                         fdb_kvs_handle *handle);

/**
 * Link a given KV Store handle into the file handle's list of
 * handles. This list may be used to auto close all child KV Store
 * handles when the file handle is closed.
 * @param fhandle - parent file handle of the ForestDB database.
 * @param handle - the newly opened KV Store handle
 * @return pointer to the newly linked node
 */
struct kvs_opened_node * _fdb_kvs_createNLinkKVHandle(fdb_file_handle *fhandle,
                                                      fdb_kvs_handle *handle);
fdb_status _fdb_kvs_close(fdb_kvs_handle *handle);
fdb_status fdb_kvs_close_all(fdb_kvs_handle *root_handle);

fdb_seqnum_t fdb_kvs_get_seqnum(struct filemgr *file,
                                fdb_kvs_id_t id);
fdb_seqnum_t fdb_kvs_get_committed_seqnum(fdb_kvs_handle *handle);

void fdb_kvs_set_seqnum(struct filemgr *file,
                        fdb_kvs_id_t id,
                        fdb_seqnum_t seqnum);

fdb_status fdb_kvs_rollback(fdb_kvs_handle **handle_ptr, fdb_seqnum_t seqnum);

/**
 * Return the smallest commit revision number that are currently being referred.
 *
 * @param handle Pointer to ForestDB KV store handle.
 * @return Header revision number and block ID.
 */
stale_header_info fdb_get_smallest_active_header(fdb_kvs_handle *handle);

INLINE size_t _fdb_get_docsize(struct docio_length len)
{
    size_t ret =
        len.keylen +
        len.metalen +
        len.bodylen_ondisk +
        sizeof(struct docio_length);

    ret += sizeof(timestamp_t);

    ret += sizeof(fdb_seqnum_t);

#ifdef __CRC32
    ret += sizeof(uint32_t);
#endif

    return ret;
}

INLINE void _fdb_import_dirty_root(fdb_kvs_handle *handle,
                                   bid_t dirty_idtree_root,
                                   bid_t dirty_seqtree_root)
{
    if (dirty_idtree_root != BLK_NOT_FOUND) {
        handle->trie->root_bid = dirty_idtree_root;
    }
    if (handle->config.seqtree_opt == FDB_SEQTREE_USE) {
        if (dirty_seqtree_root != BLK_NOT_FOUND) {
            if (handle->kvs) {
                handle->seqtrie->root_bid = dirty_seqtree_root;
            } else {
                btree_init_from_bid(handle->seqtree,
                                    handle->seqtree->blk_handle,
                                    handle->seqtree->blk_ops,
                                    handle->seqtree->kv_ops,
                                    handle->seqtree->blksize,
                                    dirty_seqtree_root);
            }
        }
    }
}

INLINE void _fdb_export_dirty_root(fdb_kvs_handle *handle,
                                   bid_t *dirty_idtree_root,
                                   bid_t *dirty_seqtree_root)
{
    *dirty_idtree_root = handle->trie->root_bid;
    if (handle->config.seqtree_opt == FDB_SEQTREE_USE) {
        if (handle->kvs) {
            *dirty_seqtree_root = handle->seqtrie->root_bid;
        } else {
            *dirty_seqtree_root = handle->seqtree->root_bid;
        }
    }
}

// 1. fetch dirty update if exist,
// 2. and assign dirty root nodes to FDB handle
INLINE void _fdb_dirty_update_ready(fdb_kvs_handle *handle,
                                    struct filemgr_dirty_update_node **prev_node,
                                    struct filemgr_dirty_update_node **new_node,
                                    bid_t *dirty_idtree_root,
                                    bid_t *dirty_seqtree_root,
                                    bool dirty_wal_flush)
{
    *prev_node = *new_node = NULL;
    *dirty_idtree_root = *dirty_seqtree_root = BLK_NOT_FOUND;

    *prev_node = filemgr_dirty_update_get_latest(handle->file);

    // discard all cached index blocks
    // to avoid data inconsistency with other writers
    btreeblk_discard_blocks(handle->bhandle);

    // create a new dirty update entry if previous one exists
    // (if we don't this, we cannot identify which block on
    //  dirty copy or actual file is more recent during the WAL flushing.)

    // on dirty wal flush, create a new dirty update entry
    // although there is no previous immutable dirty updates.

    if (*prev_node || dirty_wal_flush) {
        *new_node = filemgr_dirty_update_new_node(handle->file);
        (*new_node)->bulk_load_mode = handle->config.bulk_load_mode;
        // sync dirty root nodes
        filemgr_dirty_update_get_root(handle->file, *prev_node,
                                      dirty_idtree_root, dirty_seqtree_root);
    }
    btreeblk_set_dirty_update(handle->bhandle, *prev_node);
    btreeblk_set_dirty_update_writer(handle->bhandle, *new_node);

    // assign dirty root nodes to FDB handle
    _fdb_import_dirty_root(handle, *dirty_idtree_root, *dirty_seqtree_root);
}

// 1. get dirty root from FDB handle,
// 2. update corresponding dirty update entry,
// 3. make new_node immutable, and close previous immutable node
INLINE void _fdb_dirty_update_finalize(fdb_kvs_handle *handle,
                                       struct filemgr_dirty_update_node *prev_node,
                                       struct filemgr_dirty_update_node *new_node,
                                       bid_t *dirty_idtree_root,
                                       bid_t *dirty_seqtree_root,
                                       bool commit)
{
    // read dirty root nodes from FDB handle
    _fdb_export_dirty_root(handle, dirty_idtree_root, dirty_seqtree_root);
    // assign dirty root nodes to dirty update entry
    if (new_node) {
        filemgr_dirty_update_set_root(handle->file, new_node,
                                      *dirty_idtree_root, *dirty_seqtree_root);
    }
    // clear dirty update setting in bhandle
    btreeblk_clear_dirty_update(handle->bhandle);
    // finalize new_node
    if (new_node) {
        filemgr_dirty_update_set_immutable(handle->file, prev_node, new_node);
    }
    // close previous immutable node
    if (prev_node) {
        filemgr_dirty_update_close_node(handle->file, prev_node);
    }
    if (commit) {
        // write back new_node's dirty blocks
        filemgr_dirty_update_commit(handle->file, new_node, &handle->log_callback);
    } else {
        // if this update set is still dirty,
        // discard all cached index blocks to avoid data inconsistency.
        btreeblk_discard_blocks(handle->bhandle);
    }
}

INLINE int _lex_keycmp(void *key1, size_t keylen1, void *key2, size_t keylen2)
{
    if (keylen1 == keylen2) {
        return memcmp(key1, key2, keylen1);
    }else {
        size_t len = MIN(keylen1, keylen2);
        int cmp = memcmp(key1, key2, len);
        if (cmp != 0) return cmp;
        else {
            return (int)((int)keylen1 - (int)keylen2);
        }
    }
}

class FdbGcFunc {
public:
    using Func = std::function< void() >;

    FdbGcFunc(Func _func) : done(false), func(_func) {}
    ~FdbGcFunc() { gcNow(); }
    void gcNow() {
        if (!done) {
            func();
            done = true;
        }
    }
private:
    bool done;
    Func func;
};


#ifdef __cplusplus
}
#endif

#endif
