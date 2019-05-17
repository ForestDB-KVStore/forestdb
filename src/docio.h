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

#ifndef _JSAHN_DOCIO_H
#define _JSAHN_DOCIO_H

#include "filemgr.h"
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t keylen_t;
typedef uint32_t timestamp_t;

struct docio_handle {
    struct filemgr *file;
    bid_t curblock;
    uint32_t curpos;
    uint16_t cur_bmp_revnum_hash;
    // for buffer purpose
    bid_t lastbid;
    uint64_t lastBmpRevnum;
    void *readbuffer;
    err_log_callback *log_callback;
    bool compress_document_body;
};

#define DOCIO_NORMAL (0x00)
#define DOCIO_COMPACT (0x01)
#define DOCIO_COMPRESSED (0x02)
#define DOCIO_DELETED (0x04)
#define DOCIO_TXN_DIRTY (0x08)
#define DOCIO_TXN_COMMITTED (0x10)
#define DOCIO_SYSTEM (0x20) /* system document */
#ifdef DOCIO_LEN_STRUCT_ALIGN
    // this structure will occupy 16 bytes
    struct docio_length {
        docio_length()
            : keylen(0), metalen(0), bodylen(0), bodylen_ondisk(0)
            , flag(0x0), checksum(0x0), reserved(0x0) {}
        keylen_t keylen;
        uint16_t metalen;
        uint32_t bodylen;
        uint32_t bodylen_ondisk;
        uint8_t flag;
        uint8_t checksum;
        uint16_t reserved;
    };
#else
    // this structure will occupy 14 bytes
    struct __attribute__ ((packed)) docio_length {
        docio_length()
            : keylen(0), metalen(0), bodylen(0), bodylen_ondisk(0)
            , flag(0x0), checksum(0x0)
            {}
        keylen_t keylen;
        uint16_t metalen;
        uint32_t bodylen;
        uint32_t bodylen_ondisk;
        uint8_t flag;
        uint8_t checksum;
    };
#endif

struct docio_object {
    docio_object()
        : timestamp(0), key(nullptr), seqnum(0)
        , meta(nullptr), body(nullptr)
        {}
    struct docio_length length;
    timestamp_t timestamp;
    void *key;
    union {
        fdb_seqnum_t seqnum;
        uint64_t doc_offset;
    };
    void *meta;
    void *body;
};

fdb_status docio_init(struct docio_handle *handle,
                      struct filemgr *file,
                      bool compress_document_body);
void docio_free(struct docio_handle *handle);

bid_t docio_append_doc_raw(struct docio_handle *handle,
                           uint64_t size,
                           void *buf);

#define DOCIO_COMMIT_MARK_SIZE (sizeof(struct docio_length) + sizeof(uint64_t))
bid_t docio_append_commit_mark(struct docio_handle *handle, uint64_t doc_offset);
bid_t docio_append_doc(struct docio_handle *handle, struct docio_object *doc,
                       uint8_t deleted, uint8_t txn_enabled);
bid_t docio_append_doc_system(struct docio_handle *handle, struct docio_object *doc);

/**
 * Retrieve the length info of a KV item at a given file offset.
 *
 * @param handle Pointer to the doc I/O handle
 * @Param length Pointer to docio_length instance to be populated
 * @param offset File offset to a KV item
 * @return FDB_RESULT_SUCCESS on success
 */
fdb_status docio_read_doc_length(struct docio_handle *handle,
                                 struct docio_length *length,
                                 uint64_t offset);

/**
 * Read a key and its length at a given file offset.
 *
 * @param handle Pointer to the doc I/O handle
 * @param offset File offset to a KV item
 * @param keylen Pointer to a key length variable
 * @param keybuf Pointer to a key buffer
 * @return FDB_RESULT_SUCCESS on success
 */
fdb_status docio_read_doc_key(struct docio_handle *handle,
                              uint64_t offset,
                              keylen_t *keylen,
                              void *keybuf);

/**
 * Read a key and its metadata at a given file offset.
 *
 * @param handle Pointer to the doc I/O handle
 * @param offset File offset to a KV item
 * @param doc Pointer to docio_object instance
 * @param read_on_cache_miss Flag indicating if a disk read should be performed
 *        on cache miss
 * @return next offset right after a key and its metadata on succcessful read,
 *         otherwise, the corresponding error code is returned.
 */
int64_t docio_read_doc_key_meta(struct docio_handle *handle,
                                uint64_t offset,
                                struct docio_object *doc,
                                bool read_on_cache_miss);

/**
 * Read a KV item at a given file offset.
 *
 * @param handle Pointer to the doc I/O handle
 * @param offset File offset to a KV item
 * @param doc Pointer to docio_object instance
 * @param read_on_cache_miss Flag indicating if a disk read should be performed
 *        on cache miss
 * @return next offset right after a key and its value on succcessful read,
 *         otherwise, the corresponding error code is returned.
 */
int64_t docio_read_doc(struct docio_handle *handle,
                       uint64_t offset,
                       struct docio_object *doc,
                       bool read_on_cache_miss);

size_t docio_batch_read_docs(struct docio_handle *handle,
                             uint64_t *offset_array,
                             struct docio_object *doc_array,
                             size_t array_size,
                             size_t data_size_threshold,
                             size_t batch_size_threshold,
                             struct async_io_handle *aio_handle,
                             bool keymeta_only);

/**
 * Check if the given block is a valid document block. The bitmap revision number of
 * the document block should match the passed revision number.
 *
 * @param handle Pointer to DocIO handle.
 * @param bid ID of the block.
 * @param sb_bmp_revnum Revision number of bitmap in superblock. If the value is
 *        -1, this function does not care about revision number.
 * @return True if valid.
 */
bool docio_check_buffer(struct docio_handle *handle,
                        bid_t bid,
                        uint64_t sb_bmp_revnum);


INLINE void docio_reset(struct docio_handle *dhandle) {
    dhandle->curblock = BLK_NOT_FOUND;
}
void free_docio_object(struct docio_object *doc, uint8_t key_alloc,
                       uint8_t meta_alloc, uint8_t body_alloc);

#ifdef __cplusplus
}
#endif

#endif
