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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "btreeblock.h"
#include "fdb_internal.h"

#include "memleak.h"

#ifdef __DEBUG
#ifndef __DEBUG_BTREEBLOCK
    #undef DBG
    #undef DBGCMD
    #undef DBGSW
    #define DBG(...)
    #define DBGCMD(...)
    #define DBGSW(n, ...)
#endif
#endif

struct btreeblk_addr{
    void *addr;
    struct list_elem le;
};

struct btreeblk_block {
    bid_t bid;
    int sb_no;
    uint32_t pos;
    uint8_t dirty;
    uint8_t age;
    void *addr;
    struct list_elem le;
    struct avl_node avl;
#ifdef __BTREEBLK_BLOCKPOOL
    struct btreeblk_addr *addr_item;
#endif
};

#ifdef __BTREEBLK_READ_TREE
static int _btreeblk_bid_cmp(struct avl_node *a, struct avl_node *b, void *aux)
{
    bid_t aa_bid, bb_bid;
    struct btreeblk_block *aa, *bb;
    aa = _get_entry(a, struct btreeblk_block, avl);
    bb = _get_entry(b, struct btreeblk_block, avl);
    aa_bid = aa->bid;
    bb_bid = bb->bid;

#ifdef __BIT_CMP
    return _CMP_U64(aa_bid, bb_bid);
#else
    if (aa->bid < bb->bid) {
        return -1;
    } else if (aa->bid > bb->bid) {
        return 1;
    } else {
        return 0;
    }
#endif
}
#endif

INLINE void _btreeblk_get_aligned_block(struct btreeblk_handle *handle,
                                        struct btreeblk_block *block)
{
#ifdef __BTREEBLK_BLOCKPOOL
    struct list_elem *e;

    e = list_pop_front(&handle->blockpool);
    if (e) {
        block->addr_item = _get_entry(e, struct btreeblk_addr, le);
        block->addr = block->addr_item->addr;
        return;
    }
    // no free addr .. create
    block->addr_item = (struct btreeblk_addr *)
                       mempool_alloc(sizeof(struct btreeblk_addr));
#endif

    malloc_align(block->addr, FDB_SECTOR_SIZE, handle->file->blocksize);
}

INLINE void _btreeblk_free_aligned_block(struct btreeblk_handle *handle,
                                         struct btreeblk_block *block)
{
#ifdef __BTREEBLK_BLOCKPOOL
    if (!block->addr_item) {
        // TODO: Need to log the corresponding error message.
        return;
    }
    // sync addr & insert into pool
    block->addr_item->addr = block->addr;
    list_push_front(&handle->blockpool, &block->addr_item->le);
    block->addr_item = NULL;
    return;

#endif

    free_align(block->addr);
}

// LCOV_EXCL_START
INLINE int is_subblock(bid_t subbid)
{
    uint8_t flag;
    flag = (subbid >> (8 * (sizeof(bid_t)-2))) & 0x00ff;
    return flag;
}
// LCOV_EXCL_STOP

INLINE void bid2subbid(bid_t bid, size_t subblock_no, size_t idx, bid_t *subbid)
{
    bid_t flag;
    // to distinguish subblock_no==0 to non-subblock
    subblock_no++;
    flag = (subblock_no << 5) | idx;
    *subbid = bid | (flag << (8 * (sizeof(bid_t)-2)));
}
INLINE void subbid2bid(bid_t subbid, size_t *subblock_no, size_t *idx, bid_t *bid)
{
    uint8_t flag;
    flag = (subbid >> (8 * (sizeof(bid_t)-2))) & 0x00ff;
    *subblock_no = flag >> 5;
    // to distinguish subblock_no==0 to non-subblock
    *subblock_no -= 1;
    *idx = flag & (0x20 - 0x01);
    *bid = ((bid_t)(subbid << 16)) >> 16;
}

INLINE void * _btreeblk_alloc(void *voidhandle, bid_t *bid, int sb_no)
{
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;
    struct list_elem *e = list_end(&handle->alc_list);
    struct btreeblk_block *block;
    uint32_t curpos;

    if (e) {
        block = _get_entry(e, struct btreeblk_block, le);
        if (block->pos <= (handle->file->blocksize) - (handle->nodesize)) {
            if (filemgr_is_writable(handle->file, block->bid)) {
                curpos = block->pos;
                block->pos += (handle->nodesize);
                *bid = block->bid * handle->nnodeperblock + curpos /
                       (handle->nodesize);
                return ((uint8_t *)block->addr + curpos);
            }
        }
    }

    // allocate new block from file manager
    block = (struct btreeblk_block *)mempool_alloc(sizeof(struct btreeblk_block));
    _btreeblk_get_aligned_block(handle, block);
    if (sb_no != -1) {
        // If this block is used as a sub-block container,
        // fill it with zero bytes for easy identifying
        // which region is allocated and which region is not.
        memset(block->addr, 0x0, handle->nodesize);
    }
    block->sb_no = sb_no;
    block->pos = handle->nodesize;
    block->bid = filemgr_alloc(handle->file, handle->log_callback);
    block->dirty = 1;
    block->age = 0;

    // If a block is allocated but not written back into file (due to
    // various reasons), the corresponding byte offset in the file is filled
    // with garbage data so that it causes various unexpected behaviors.
    // To avoid this issue, populate block cache for the given BID before use it.
    uint8_t marker = BLK_MARKER_BNODE;
    filemgr_write_offset(handle->file, block->bid, handle->file->blocksize - 1,
                         1, &marker, false, handle->log_callback);

#ifdef __CRC32
    memset((uint8_t *)block->addr + handle->nodesize - BLK_MARKER_SIZE,
           BLK_MARKER_BNODE, BLK_MARKER_SIZE);
#endif

    // btree bid differs to filemgr bid
    *bid = block->bid * handle->nnodeperblock;
    list_push_back(&handle->alc_list, &block->le);

    handle->nlivenodes++;
    handle->ndeltanodes++;

    return block->addr;
}
void * btreeblk_alloc(void *voidhandle, bid_t *bid) {
    return _btreeblk_alloc(voidhandle, bid, -1);
}


#ifdef __ENDIAN_SAFE
INLINE void _btreeblk_encode(struct btreeblk_handle *handle,
                             struct btreeblk_block *block)
{
    size_t i, nsb, sb_size, offset;
    void *addr;
    struct bnode *node;

    for (offset=0; offset<handle->nnodeperblock; ++offset) {
        if (block->sb_no > -1) {
            nsb = handle->sb[block->sb_no].nblocks;
            sb_size = handle->sb[block->sb_no].sb_size;
        } else {
            nsb = 1;
            sb_size = 0;
        }

        for (i=0;i<nsb;++i) {
            addr = (uint8_t*)block->addr +
                   (handle->nodesize) * offset +
                   sb_size * i;
#ifdef _BTREE_HAS_MULTIPLE_BNODES
            size_t j, n;
            struct bnode **node_arr;
            node_arr = btree_get_bnode_array(addr, &n);
            for (j=0;j<n;++j){
                node = node_arr[j];
                node->kvsize = _endian_encode(node->kvsize);
                node->flag = _endian_encode(node->flag);
                node->level = _endian_encode(node->level);
                node->nentry = _endian_encode(node->nentry);
            }
            free(node_arr);
#else
            node = btree_get_bnode(addr);
            node->kvsize = _endian_encode(node->kvsize);
            node->flag = _endian_encode(node->flag);
            node->level = _endian_encode(node->level);
            node->nentry = _endian_encode(node->nentry);
#endif
        }
    }
}
INLINE void _btreeblk_decode(struct btreeblk_handle *handle,
                             struct btreeblk_block *block)
{
    size_t i, nsb, sb_size, offset;
    void *addr;
    struct bnode *node;

    for (offset=0; offset<handle->nnodeperblock; ++offset) {
        if (block->sb_no > -1) {
            nsb = handle->sb[block->sb_no].nblocks;
            sb_size = handle->sb[block->sb_no].sb_size;
        } else {
            nsb = 1;
            sb_size = 0;
        }

        for (i=0;i<nsb;++i) {
            addr = (uint8_t*)block->addr +
                   (handle->nodesize) * offset +
                   sb_size * i;
#ifdef _BTREE_HAS_MULTIPLE_BNODES
            size_t j, n;
            struct bnode **node_arr;
            node_arr = btree_get_bnode_array(addr, &n);
            for (j=0;j<n;++j){
                node = node_arr[j];
                node->kvsize = _endian_decode(node->kvsize);
                node->flag = _endian_decode(node->flag);
                node->level = _endian_decode(node->level);
                node->nentry = _endian_decode(node->nentry);
            }
            free(node_arr);
#else
            node = btree_get_bnode(addr);
            node->kvsize = _endian_decode(node->kvsize);
            node->flag = _endian_decode(node->flag);
            node->level = _endian_decode(node->level);
            node->nentry = _endian_decode(node->nentry);
#endif
        }
    }
}
#else
#define _btreeblk_encode(a,b)
#define _btreeblk_decode(a,b)
#endif

INLINE void _btreeblk_free_dirty_block(struct btreeblk_handle *handle,
                                       struct btreeblk_block *block);

INLINE void * _btreeblk_read(void *voidhandle, bid_t bid, int sb_no)
{
    struct list_elem *elm = NULL;
    struct btreeblk_block *block = NULL;
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;
    bid_t _bid, filebid;
    int subblock;
    int offset;
    size_t sb, idx;

    sb = idx = 0;
    subbid2bid(bid, &sb, &idx, &_bid);
    subblock = is_subblock(bid);
    filebid = _bid / handle->nnodeperblock;
    offset = _bid % handle->nnodeperblock;

    // check whether the block is in current lists
    // read list (clean or dirty)
#ifdef __BTREEBLK_READ_TREE
    // AVL-tree
    // check first 3 elements in the list first,
    // and then retrieve AVL-tree
    size_t count = 0;
    for (elm = list_begin(&handle->read_list);
         (elm && count < 3); elm = list_next(elm)) {
        block = _get_entry(elm, struct btreeblk_block, le);
        if (block->bid == filebid) {
            block->age = 0;
            // move the elements to the front
            list_remove(&handle->read_list, &block->le);
            list_push_front(&handle->read_list, &block->le);
            if (subblock) {
                return (uint8_t *)block->addr +
                       (handle->nodesize) * offset +
                       handle->sb[sb].sb_size * idx;
            } else {
                return (uint8_t *)block->addr +
                       (handle->nodesize) * offset;
            }
        }
        count++;
    }

    struct btreeblk_block query;
    query.bid = filebid;
    struct avl_node *a;
    a = avl_search(&handle->read_tree, &query.avl, _btreeblk_bid_cmp);
    if (a) { // cache hit
        block = _get_entry(a, struct btreeblk_block, avl);
        block->age = 0;
        // move the elements to the front
        list_remove(&handle->read_list, &block->le);
        list_push_front(&handle->read_list, &block->le);
        if (subblock) {
            return (uint8_t *)block->addr +
                   (handle->nodesize) * offset +
                   handle->sb[sb].sb_size * idx;
        } else {
            return (uint8_t *)block->addr +
                   (handle->nodesize) * offset;
        }
    }
#else
    // list
    for (elm = list_begin(&handle->read_list); elm; elm = list_next(elm)) {
        block = _get_entry(elm, struct btreeblk_block, le);
        if (block->bid == filebid) {
            block->age = 0;
            if (subblock) {
                return (uint8_t *)block->addr +
                       (handle->nodesize) * offset +
                       handle->sb[sb].sb_size * idx;
            } else {
                return (uint8_t *)block->addr +
                       (handle->nodesize) * offset;
            }
        }
    }
#endif

    // allocation list (dirty)
    for (elm = list_begin(&handle->alc_list); elm; elm = list_next(elm)) {
        block = _get_entry(elm, struct btreeblk_block, le);
        if (block->bid == filebid &&
            block->pos >= (handle->nodesize) * offset) {
            block->age = 0;
            if (subblock) {
                return (uint8_t *)block->addr +
                       (handle->nodesize) * offset +
                       handle->sb[sb].sb_size * idx;
            } else {
                return (uint8_t *)block->addr +
                       (handle->nodesize) * offset;
            }
        }
    }

    // there is no block in lists
    // if miss, read from file and add item into read list
    block = (struct btreeblk_block *)mempool_alloc(sizeof(struct btreeblk_block));
    block->sb_no = (subblock)?(sb):(sb_no);
    block->pos = (handle->file->blocksize);
    block->bid = filebid;
    block->dirty = 0;
    block->age = 0;

    _btreeblk_get_aligned_block(handle, block);

    fdb_status status;
    if (handle->dirty_update || handle->dirty_update_writer) {
        // read from the given dirty update entry
        status = filemgr_read_dirty(handle->file, block->bid, block->addr,
                                    handle->dirty_update, handle->dirty_update_writer,
                                    handle->log_callback, true);
    } else {
        // normal read
        status = filemgr_read(handle->file, block->bid, block->addr,
                              handle->log_callback, true);
    }
    if (status != FDB_RESULT_SUCCESS) {
        fdb_log(handle->log_callback, FDB_LOG_ERROR, status,
                "Failed to read the B+-Tree block (block id: %" _F64
                ", block address: %p)", block->bid, block->addr);
        _btreeblk_free_aligned_block(handle, block);
        mempool_free(block);
        return NULL;
    }

    _btreeblk_decode(handle, block);

    list_push_front(&handle->read_list, &block->le);
#ifdef __BTREEBLK_READ_TREE
    avl_insert(&handle->read_tree, &block->avl, _btreeblk_bid_cmp);
#endif

    if (subblock) {
        return (uint8_t *)block->addr +
               (handle->nodesize) * offset +
               handle->sb[sb].sb_size * idx;
    } else {
        return (uint8_t *)block->addr + (handle->nodesize) * offset;
    }
}

void * btreeblk_read(void *voidhandle, bid_t bid)
{
    return _btreeblk_read(voidhandle, bid, -1);
}

INLINE void _btreeblk_add_stale_block(struct btreeblk_handle *handle,
                                 uint64_t pos,
                                 uint32_t len)
{
    filemgr_add_stale_block(handle->file, pos, len);
}

void btreeblk_set_dirty(void *voidhandle, bid_t bid);
void * btreeblk_move(void *voidhandle, bid_t bid, bid_t *new_bid)
{
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;
    void *old_addr, *new_addr;
    bid_t _old_bid, _new_bid;
    int i, subblock;
    size_t sb, old_sb_idx, new_sb_idx;

    old_addr = new_addr = NULL;
    sb = old_sb_idx = 0;
    subbid2bid(bid, &sb, &old_sb_idx, &_old_bid);
    subblock = is_subblock(bid);

    int convert_to_normal = 0;
#ifndef __BTREEBLK_SUBBLOCK
    convert_to_normal = 1;
#endif

    if (!subblock) {
        // normal block
        old_addr = btreeblk_read(voidhandle, bid);
        new_addr = btreeblk_alloc(voidhandle, new_bid);
        handle->nlivenodes--;

        // move
        memcpy(new_addr, old_addr, (handle->nodesize));

        // the entire block becomes stale
        _btreeblk_add_stale_block(handle, bid * handle->nodesize, handle->nodesize);

        return new_addr;
    } else {
        // subblock

        // move the target subblock
        // into the current subblock set
        old_addr = _btreeblk_read(voidhandle, _old_bid, sb);

        new_sb_idx = handle->sb[sb].nblocks;
        for (i=0;i<handle->sb[sb].nblocks;++i){
            if (handle->sb[sb].bitmap[i] == 0) {
                new_sb_idx = i;
                break;
            }
        }
        if ( handle->sb[sb].bid == BLK_NOT_FOUND ||
             new_sb_idx == handle->sb[sb].nblocks ||
             !filemgr_is_writable(handle->file, handle->sb[sb].bid) ||
             convert_to_normal ) {
            // There is no free slot in the parent block, OR
            // the parent block is not writable, OR
            // `convert_to_normal` flag is on.

            // Mark all unused subblocks in the current parent block as stale
            if (handle->sb[sb].bid != BLK_NOT_FOUND) {
                for (i=0; i<handle->sb[sb].nblocks; ++i) {
                    if (handle->sb[sb].bitmap[i] == 0) {
                        _btreeblk_add_stale_block
                        ( handle,
                          ( (handle->sb[sb].bid * handle->nodesize) +
                            (i * handle->sb[sb].sb_size) ),
                          handle->sb[sb].sb_size );
                    }
                }
            }

            if (convert_to_normal) {
                // Whole block.
                new_addr = btreeblk_alloc(voidhandle, &_new_bid);
                handle->nlivenodes--;
            } else {
                // Allocate new parent block.
                new_addr = _btreeblk_alloc(voidhandle, &_new_bid, sb);
                handle->nlivenodes--;
                handle->sb[sb].bid = _new_bid;
                memset(handle->sb[sb].bitmap, 0, handle->sb[sb].nblocks);
            }
            new_sb_idx = 0;

        } else {
            // just append to the current block
            new_addr = _btreeblk_read(voidhandle, handle->sb[sb].bid, sb);
        }

        if (convert_to_normal) {
            *new_bid = _new_bid;
        } else {
            handle->sb[sb].bitmap[new_sb_idx] = 1;
            bid2subbid(handle->sb[sb].bid, sb, new_sb_idx, new_bid);
            btreeblk_set_dirty(voidhandle, handle->sb[sb].bid);
        }

        // move
        memcpy( (uint8_t*)new_addr + handle->sb[sb].sb_size * new_sb_idx,
                (uint8_t*)old_addr + handle->sb[sb].sb_size * old_sb_idx,
                handle->sb[sb].sb_size );

        // Also mark the target (old) subblock as stale
        _btreeblk_add_stale_block
        ( handle,
          (_old_bid * handle->nodesize) + (old_sb_idx * handle->sb[sb].sb_size),
           handle->sb[sb].sb_size );

        return (uint8_t*)new_addr + handle->sb[sb].sb_size * new_sb_idx;
    }
}

// LCOV_EXCL_START
void btreeblk_remove(void *voidhandle, bid_t bid)
{
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;
    bid_t _bid;
    int i, subblock, nitems;
    size_t sb, idx;

    sb = idx = 0;
    subbid2bid(bid, &sb, &idx, &_bid);
    subblock = is_subblock(bid);

    if (subblock) {
        // subblock
        if (handle->sb[sb].bid == _bid) {
            // erase bitmap
            handle->sb[sb].bitmap[idx] = 0;
            // if all slots are empty, invalidate the block
            nitems = 0;
            for (i=0;i<handle->sb[sb].nblocks;++i){
                if (handle->sb[sb].bitmap) {
                    nitems++;
                }
            }
            if (nitems == 0) {
                handle->sb[sb].bid = BLK_NOT_FOUND;
                handle->nlivenodes--;
                _btreeblk_add_stale_block(handle,
                                          _bid * handle->nodesize,
                                          handle->nodesize);
            }
        }
    } else {
        // normal block
        handle->nlivenodes--;
        _btreeblk_add_stale_block(handle,
                                  _bid * handle->nodesize,
                                  handle->nodesize);
    }
}
// LCOV_EXCL_STOP

int btreeblk_is_writable(void *voidhandle, bid_t bid)
{
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;
    bid_t _bid;
    bid_t filebid;
    size_t sb, idx;

    sb = idx = 0;
    subbid2bid(bid, &sb, &idx, &_bid);
    filebid = _bid / handle->nnodeperblock;

    return filemgr_is_writable(handle->file, filebid);
}

void btreeblk_set_dirty(void *voidhandle, bid_t bid)
{
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;
    struct list_elem *e;
    struct btreeblk_block *block;
    bid_t _bid;
    bid_t filebid;
    size_t sb, idx;

    sb = idx = 0;
    subbid2bid(bid, &sb, &idx, &_bid);
    filebid = _bid / handle->nnodeperblock;

#ifdef __BTREEBLK_READ_TREE
    // AVL-tree
    struct btreeblk_block query;
    query.bid = filebid;
    struct avl_node *a;
    a = avl_search(&handle->read_tree, &query.avl, _btreeblk_bid_cmp);
    if (a) {
        block = _get_entry(a, struct btreeblk_block, avl);
        block->dirty = 1;
    }
#else
    // list
    e = list_begin(&handle->read_list);
    while(e){
        block = _get_entry(e, struct btreeblk_block, le);
        if (block->bid == filebid) {
            block->dirty = 1;
            break;
        }
        e = list_next(e);
    }
#endif
}

static void _btreeblk_set_sb_no(void *voidhandle, bid_t bid, int sb_no)
{
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;
    struct list_elem *e;
    struct btreeblk_block *block;
    bid_t _bid;
    bid_t filebid;
    size_t sb, idx;

    sb = idx = 0;
    subbid2bid(bid, &sb, &idx, &_bid);
    filebid = _bid / handle->nnodeperblock;

    e = list_begin(&handle->alc_list);
    while(e){
        block = _get_entry(e, struct btreeblk_block, le);
        if (block->bid == filebid) {
            block->sb_no = sb_no;
            return;
        }
        e = list_next(e);
    }

#ifdef __BTREEBLK_READ_TREE
    // AVL-tree
    struct btreeblk_block query;
    query.bid = filebid;
    struct avl_node *a;
    a = avl_search(&handle->read_tree, &query.avl, _btreeblk_bid_cmp);
    if (a) {
        block = _get_entry(a, struct btreeblk_block, avl);
        block->sb_no = sb_no;
    }
#else
    // list
    e = list_begin(&handle->read_list);
    while(e){
        block = _get_entry(e, struct btreeblk_block, le);
        if (block->bid == filebid) {
            block->sb_no = sb_no;
            return;
        }
        e = list_next(e);
    }
#endif
}

size_t btreeblk_get_size(void *voidhandle, bid_t bid)
{
    bid_t _bid;
    size_t sb, idx;
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;

    if (is_subblock(bid) && bid != BLK_NOT_FOUND) {
        subbid2bid(bid, &sb, &idx, &_bid);
        return handle->sb[sb].sb_size;
    } else {
        return handle->nodesize;
    }
}

void * btreeblk_alloc_sub(void *voidhandle, bid_t *bid)
{
    int i;
    void *addr;
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;

    if (handle->nsb == 0) {
        return btreeblk_alloc(voidhandle, bid);
    }

    // check current block is available
    if (handle->sb[0].bid != BLK_NOT_FOUND) {
        if (filemgr_is_writable(handle->file, handle->sb[0].bid)) {
            // check if there is an empty slot
            for (i=0;i<handle->sb[0].nblocks;++i){
                if (handle->sb[0].bitmap[i] == 0) {
                    // return subblock
                    handle->sb[0].bitmap[i] = 1;
                    bid2subbid(handle->sb[0].bid, 0, i, bid);
                    addr = _btreeblk_read(voidhandle, handle->sb[0].bid, 0);
                    btreeblk_set_dirty(voidhandle, handle->sb[0].bid);
                    return (void*)
                           ((uint8_t*)addr +
                            handle->sb[0].sb_size * i);
                }
            }
        } else {
            // we have to mark all unused slots as stale
            size_t idx;
            for (idx=0; idx<handle->sb[0].nblocks; ++idx) {
                if (handle->sb[0].bitmap[idx] == 0) {
                    _btreeblk_add_stale_block(handle,
                        (handle->sb[0].bid * handle->nodesize)
                            + (idx * handle->sb[0].sb_size),
                        handle->sb[0].sb_size);
                }
            }
        }
    }

    // existing subblock cannot be used .. give it up & allocate new one
    addr = _btreeblk_alloc(voidhandle, &handle->sb[0].bid, 0);
    memset(handle->sb[0].bitmap, 0, handle->sb[0].nblocks);
    i = 0;
    handle->sb[0].bitmap[i] = 1;
    bid2subbid(handle->sb[0].bid, 0, i, bid);
    return (void*)((uint8_t*)addr + handle->sb[0].sb_size * i);
}

void * btreeblk_enlarge_node(void *voidhandle,
                             bid_t old_bid,
                             size_t req_size,
                             bid_t *new_bid)
{
    uint32_t i;
    bid_t bid;
    size_t src_sb, src_idx, src_nitems;
    size_t dst_sb, dst_idx, dst_nitems;
    void *src_addr, *dst_addr;
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;

    if (!is_subblock(old_bid)) {
        return NULL;
    }
    src_addr = dst_addr = NULL;
    subbid2bid(old_bid, &src_sb, &src_idx, &bid);

    dst_sb = 0;
    // find sublock that can accommodate req_size
    for (i=src_sb+1; i<handle->nsb; ++i){
        if (handle->sb[i].sb_size > req_size) {
            dst_sb = i;
            break;
        }
    }

    src_nitems = 0;
    for (i=0;i<handle->sb[src_sb].nblocks;++i){
        if (handle->sb[src_sb].bitmap[i]) {
            src_nitems++;
        }
    }

    dst_nitems = 0;
    if (dst_sb > 0) {
        dst_idx = handle->sb[dst_sb].nblocks;
        for (i=0;i<handle->sb[dst_sb].nblocks;++i){
            if (handle->sb[dst_sb].bitmap[i]) {
                dst_nitems++;
            } else if (dst_idx == handle->sb[dst_sb].nblocks) {
                dst_idx = i;
            }
        }
    }

    if (dst_nitems == 0) {
        // destination block is empty
        dst_idx = 0;
        if (src_nitems == 1 &&
            bid == handle->sb[src_sb].bid &&
            filemgr_is_writable(handle->file, bid)) {
            //2 case 1
            // if there's only one subblock in the source block, and
            // the source block is still writable and allocable,
            // then switch source block to destination block
            src_addr = _btreeblk_read(voidhandle, bid, src_sb);
            dst_addr = src_addr;
            if (dst_sb > 0) {
                handle->sb[dst_sb].bid = handle->sb[src_sb].bid;
            } else {
                *new_bid = handle->sb[src_sb].bid;
            }
            btreeblk_set_dirty(voidhandle, handle->sb[src_sb].bid);
            // we MUST change block->sb_no value since subblock is switched.
            // dst_sb == 0: regular block, otherwise: sub-block
            _btreeblk_set_sb_no(voidhandle, handle->sb[src_sb].bid,
                                ((dst_sb)?(dst_sb):(-1)));

            if (src_idx > 0 || dst_addr != src_addr) {
                // move node to the beginning of the block
                memmove(dst_addr,
                        (uint8_t*)src_addr + handle->sb[src_sb].sb_size * src_idx,
                        handle->sb[src_sb].sb_size);
            }
            if (dst_sb > 0) {
                handle->sb[dst_sb].bitmap[dst_idx] = 1;
            }
            if (bid == handle->sb[src_sb].bid) {
                // remove existing source block info
                handle->sb[src_sb].bid = BLK_NOT_FOUND;
                memset(handle->sb[src_sb].bitmap, 0,
                       handle->sb[src_sb].nblocks);
            }

        } else {
            //2 case 2
            // if there are more than one subblock in the source block,
            // or no more subblock is allocable from the current source block,
            // then allocate a new destination block and move the target subblock only.
            src_addr = _btreeblk_read(voidhandle, bid, src_sb);

            if (dst_sb > 0) {
                // case 2-1: enlarged block will be also a subblock
                dst_addr = _btreeblk_alloc(voidhandle, &handle->sb[dst_sb].bid, dst_sb);
                memcpy((uint8_t*)dst_addr + handle->sb[dst_sb].sb_size * dst_idx,
                       (uint8_t*)src_addr + handle->sb[src_sb].sb_size * src_idx,
                       handle->sb[src_sb].sb_size);
                handle->sb[dst_sb].bitmap[dst_idx] = 1;
            } else {
                // case 2-2: enlarged block will be a regular block
                dst_addr = btreeblk_alloc(voidhandle, new_bid);
                memcpy((uint8_t*)dst_addr,
                       (uint8_t*)src_addr + handle->sb[src_sb].sb_size * src_idx,
                       handle->sb[src_sb].sb_size);
            }

            // Mark the source subblock as stale.
            if (bid == handle->sb[src_sb].bid) {
                // The current source block may be still allocable.
                // Remove the corresponding bitmap from the source bitmap.
                // All unused subblocks will be marked as stale when this block
                // becomes immutable.
                handle->sb[src_sb].bitmap[src_idx] = 0;

                // TODO: what if FDB handle is closed without fdb_commit() ?
            } else if (bid != BLK_NOT_FOUND) {
                // The current source block will not be used for allocation anymore.
                // Mark the corresponding subblock as stale.
                _btreeblk_add_stale_block(handle,
                    (bid * handle->nodesize) + (src_idx * handle->sb[src_sb].sb_size),
                    handle->sb[src_sb].sb_size);
            }
        }
    } else {
        //2 case 3
        // destination block exists
        // (happens only when the destination block is
        //  a parent block of subblock set)
        src_addr = _btreeblk_read(voidhandle, bid, src_sb);
        if (filemgr_is_writable(handle->file, handle->sb[dst_sb].bid) &&
            dst_idx != handle->sb[dst_sb].nblocks) {
            // case 3-1
            dst_addr = _btreeblk_read(voidhandle, handle->sb[dst_sb].bid, dst_sb);
            btreeblk_set_dirty(voidhandle, handle->sb[dst_sb].bid);
        } else {
            // case 3-2: allocate new destination block
            dst_addr = _btreeblk_alloc(voidhandle, &handle->sb[dst_sb].bid, dst_sb);
            memset(handle->sb[dst_sb].bitmap, 0, handle->sb[dst_sb].nblocks);
            dst_idx = 0;
        }

        memcpy((uint8_t*)dst_addr + handle->sb[dst_sb].sb_size * dst_idx,
               (uint8_t*)src_addr + handle->sb[src_sb].sb_size * src_idx,
               handle->sb[src_sb].sb_size);
        handle->sb[dst_sb].bitmap[dst_idx] = 1;

        // Mark the source subblock as stale.
        if (bid == handle->sb[src_sb].bid) {
            // The current source block may be still allocable.
            // Remove the corresponding bitmap from the source bitmap.
            // All unused subblocks will be marked as stale when this block
            // becomes immutable.
            handle->sb[src_sb].bitmap[src_idx] = 0;
        } else if (handle->sb[src_sb].bid != BLK_NOT_FOUND) {
            // The current source block will not be used for allocation anymore.
            // Mark the corresponding subblock as stale.
            _btreeblk_add_stale_block(handle,
                (bid * handle->nodesize) + (src_idx * handle->sb[src_sb].sb_size),
                handle->sb[src_sb].sb_size);
        }

    }

    if (dst_sb > 0) {
        // sub block
        bid2subbid(handle->sb[dst_sb].bid, dst_sb, dst_idx, new_bid);
        return (uint8_t*)dst_addr + handle->sb[dst_sb].sb_size * dst_idx;
    } else {
        // whole block
        return dst_addr;
    }
}

INLINE void _btreeblk_free_dirty_block(struct btreeblk_handle *handle,
                                       struct btreeblk_block *block)
{
    _btreeblk_free_aligned_block(handle, block);
    mempool_free(block);
}

INLINE fdb_status _btreeblk_write_dirty_block(struct btreeblk_handle *handle,
                                        struct btreeblk_block *block)
{
    fdb_status status;
    //2 MUST BE modified to support multiple nodes in a block

    _btreeblk_encode(handle, block);
    if (handle->dirty_update_writer) {
        // dirty update is in-progress
        status = filemgr_write_dirty(handle->file, block->bid, block->addr,
                                     handle->dirty_update_writer,
                                     handle->log_callback);
    } else {
        // normal write into file
        status = filemgr_write(handle->file, block->bid, block->addr,
                               handle->log_callback);
    }
    if (status != FDB_RESULT_SUCCESS) {
        fdb_log(handle->log_callback, FDB_LOG_ERROR, status,
                "Failed to write the B+-Tree block (block id: %" _F64
                ", block address: %p)", block->bid, block->addr);
    }
    _btreeblk_decode(handle, block);
    return status;
}

fdb_status btreeblk_operation_end(void *voidhandle)
{
    // flush and write all items in allocation list
    struct btreeblk_handle *handle = (struct btreeblk_handle *)voidhandle;
    struct list_elem *e;
    struct btreeblk_block *block;
    int writable;
    fdb_status status = FDB_RESULT_SUCCESS;

    // write and free items in allocation list
    e = list_begin(&handle->alc_list);
    while(e){
        block = _get_entry(e, struct btreeblk_block, le);
        writable = filemgr_is_writable(handle->file, block->bid);
        if (writable) {
            status = _btreeblk_write_dirty_block(handle, block);
            if (status != FDB_RESULT_SUCCESS) {
                return status;
            }
        } else {
            return FDB_RESULT_WRITE_FAIL;
        }

        if (block->pos + (handle->nodesize) > (handle->file->blocksize) || !writable) {
            // remove from alc_list and insert into read list
            e = list_remove(&handle->alc_list, &block->le);
            block->dirty = 0;
            list_push_front(&handle->read_list, &block->le);
#ifdef __BTREEBLK_READ_TREE
            avl_insert(&handle->read_tree, &block->avl, _btreeblk_bid_cmp);
#endif
        }else {
            // reserve the block when there is enough space and the block is writable
            e = list_next(e);
        }
    }

    // free items in read list
#ifdef __BTREEBLK_READ_TREE
    // AVL-tree
    struct avl_node *a;
    a = avl_first(&handle->read_tree);
    while (a) {
        block = _get_entry(a, struct btreeblk_block, avl);
        a = avl_next(a);

        if (block->dirty) {
            // write back only when the block is modified
            status = _btreeblk_write_dirty_block(handle, block);
            if (status != FDB_RESULT_SUCCESS) {
                return status;
            }
            block->dirty = 0;
        }

        if (block->age >= BTREEBLK_AGE_LIMIT) {
            list_remove(&handle->read_list, &block->le);
            avl_remove(&handle->read_tree, &block->avl);
            _btreeblk_free_dirty_block(handle, block);
        } else {
            block->age++;
        }
    }
#else
    // list
    e = list_begin(&handle->read_list);
    while(e){
        block = _get_entry(e, struct btreeblk_block, le);

        if (block->dirty) {
            // write back only when the block is modified
            status = _btreeblk_write_dirty_block(handle, block);
            if (status != FDB_RESULT_SUCCESS) {
                return status;
            }
            block->dirty = 0;
        }

        if (block->age >= BTREEBLK_AGE_LIMIT) {
            e = list_remove(&handle->read_list, &block->le);
            _btreeblk_free_dirty_block(handle, block);
        } else {
            block->age++;
            e = list_next(e);
        }
    }
#endif

    return status;
}

void btreeblk_discard_blocks(struct btreeblk_handle *handle)
{
    // discard all writable blocks in the read list
    struct list_elem *e;
    struct btreeblk_block *block;

    // free items in read list
#ifdef __BTREEBLK_READ_TREE
    // AVL-tree
    struct avl_node *a;
    a = avl_first(&handle->read_tree);
    while (a) {
        block = _get_entry(a, struct btreeblk_block, avl);
        a = avl_next(a);

        list_remove(&handle->read_list, &block->le);
        avl_remove(&handle->read_tree, &block->avl);
        _btreeblk_free_dirty_block(handle, block);
    }
#else
    // list
    e = list_begin(&handle->read_list);
    while(e){
        block = _get_entry(e, struct btreeblk_block, le);
        e = list_next(&block->le);

        list_remove(&handle->read_list, &block->le);
        _btreeblk_free_dirty_block(handle, block);
    }
#endif
}

#ifdef __BTREEBLK_SUBBLOCK
    struct btree_blk_ops btreeblk_ops = {
        btreeblk_alloc,
        btreeblk_alloc_sub,
        btreeblk_enlarge_node,
        btreeblk_read,
        btreeblk_move,
        btreeblk_remove,
        btreeblk_is_writable,
        btreeblk_get_size,
        btreeblk_set_dirty,
        NULL
    };
#else
    struct btree_blk_ops btreeblk_ops = {
        btreeblk_alloc,
        NULL,
        NULL,
        btreeblk_read,
        btreeblk_move,
        btreeblk_remove,
        btreeblk_is_writable,
        btreeblk_get_size,
        btreeblk_set_dirty,
        NULL
    };
#endif

struct btree_blk_ops *btreeblk_get_ops()
{
    return &btreeblk_ops;
}

void btreeblk_init(struct btreeblk_handle *handle, struct filemgr *file,
                   uint32_t nodesize)
{
    uint32_t i;
    uint32_t _nodesize;

    handle->file = file;
    handle->nodesize = nodesize;
    handle->nnodeperblock = handle->file->blocksize / handle->nodesize;
    handle->nlivenodes = 0;
    handle->ndeltanodes = 0;
    handle->dirty_update = NULL;
    handle->dirty_update_writer = NULL;

    list_init(&handle->alc_list);
    list_init(&handle->read_list);

#ifdef __BTREEBLK_READ_TREE
    avl_init(&handle->read_tree, NULL);
#endif

#ifdef __BTREEBLK_BLOCKPOOL
    list_init(&handle->blockpool);
#endif

    // compute # subblock sets
    _nodesize = BTREEBLK_MIN_SUBBLOCK;
    for (i=0; (_nodesize < nodesize && i<5); ++i){
        _nodesize = _nodesize << 1;
    }
    handle->nsb = i;
    if (handle->nsb) {
        handle->sb = (struct btreeblk_subblocks*)
                     malloc(sizeof(struct btreeblk_subblocks) * handle->nsb);
        // initialize each subblock set
        _nodesize = BTREEBLK_MIN_SUBBLOCK;
        for (i=0;i<handle->nsb;++i){
            handle->sb[i].bid = BLK_NOT_FOUND;
            handle->sb[i].sb_size = _nodesize;
            handle->sb[i].nblocks = nodesize / _nodesize;
            handle->sb[i].bitmap = (uint8_t*)malloc(handle->sb[i].nblocks);
            memset(handle->sb[i].bitmap, 0, handle->sb[i].nblocks);
            _nodesize = _nodesize << 1;
        }
    } else {
        handle->sb = NULL;
    }
}

void btreeblk_reset_subblock_info(struct btreeblk_handle *handle)
{
#ifdef __BTREEBLK_SUBBLOCK
    uint32_t sb_no, idx;

    for (sb_no=0;sb_no<handle->nsb;++sb_no){
        if (handle->sb[sb_no].bid != BLK_NOT_FOUND) {
            // first of all, make all unused subblocks as stale
            for (idx=0; idx<handle->sb[sb_no].nblocks; ++idx) {
                if (handle->sb[sb_no].bitmap[idx] == 0) {
                    _btreeblk_add_stale_block(handle,
                        (handle->sb[sb_no].bid * handle->nodesize)
                            + (idx * handle->sb[sb_no].sb_size),
                        handle->sb[sb_no].sb_size);
                }
            }
            handle->sb[sb_no].bid = BLK_NOT_FOUND;
        }
        // clear all info in each subblock set
        memset(handle->sb[sb_no].bitmap, 0, handle->sb[sb_no].nblocks);
    }
#endif
}

// shutdown
void btreeblk_free(struct btreeblk_handle *handle)
{
    struct list_elem *e;
    struct btreeblk_block *block;

    // free all blocks in alc list
    e = list_begin(&handle->alc_list);
    while(e) {
        block = _get_entry(e, struct btreeblk_block, le);
        e = list_remove(&handle->alc_list, &block->le);
        _btreeblk_free_dirty_block(handle, block);
    }

    // free all blocks in read list
#ifdef __BTREEBLK_READ_TREE
    // AVL tree
    struct avl_node *a;
    a = avl_first(&handle->read_tree);
    while (a) {
        block = _get_entry(a, struct btreeblk_block, avl);
        a = avl_next(a);
        avl_remove(&handle->read_tree, &block->avl);
        _btreeblk_free_dirty_block(handle, block);
    }
#else
    // linked list
    e = list_begin(&handle->read_list);
    while(e) {
        block = _get_entry(e, struct btreeblk_block, le);
        e = list_remove(&handle->read_list, &block->le);
        _btreeblk_free_dirty_block(handle, block);
    }
#endif

#ifdef __BTREEBLK_BLOCKPOOL
    // free all blocks in the block pool
    struct btreeblk_addr *item;

    e = list_begin(&handle->blockpool);
    while(e){
        item = _get_entry(e, struct btreeblk_addr, le);
        e = list_next(e);

        free_align(item->addr);
        mempool_free(item);
    }
#endif

    uint32_t i;
    for (i=0;i<handle->nsb;++i){
        free(handle->sb[i].bitmap);
    }
    free(handle->sb);
}

fdb_status btreeblk_end(struct btreeblk_handle *handle)
{
    struct list_elem *e;
    struct btreeblk_block *block;
    fdb_status status = FDB_RESULT_SUCCESS;

    // flush all dirty items
    status = btreeblk_operation_end((void *)handle);
    if (status != FDB_RESULT_SUCCESS) {
        return status;
    }

    // remove all items in lists
    e = list_begin(&handle->alc_list);
    while(e) {
        block = _get_entry(e, struct btreeblk_block, le);
        e = list_remove(&handle->alc_list, &block->le);

        block->dirty = 0;
        list_push_front(&handle->read_list, &block->le);
#ifdef __BTREEBLK_READ_TREE
        avl_insert(&handle->read_tree, &block->avl, _btreeblk_bid_cmp);
#endif
    }
    return status;
}
