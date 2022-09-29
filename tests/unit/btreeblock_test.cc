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

#include "filemgr.h"
#include "filemgr_ops.h"
#include "btreeblock.h"
#include "btree.h"
#include "btree_kv.h"
#include "test.h"

#include "memleak.h"

void print_btree(struct btree *btree, void *key, void *value)
{
    fprintf(stderr, "(%" _F64 " %" _F64 ")", *(uint64_t*)key, *(uint64_t*)value);
}

void basic_test()
{
    TEST_INIT();

    int ksize = 8;
    int vsize = 8;
    int nodesize = (ksize + vsize)*4 + sizeof(struct bnode);
    int blocksize = nodesize * 2;
    struct filemgr *file;
    struct btreeblk_handle btree_handle;
    struct btree btree;
    struct filemgr_config config;
    int i, r;
    uint64_t k,v;
    char *fname = (char *) "./btreeblock_testfile";

    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    memset(&config, 0, sizeof(config));
    config.blocksize = blocksize;
    config.ncacheblock = 0;
    config.flag = 0x0;
    config.options = FILEMGR_CREATE;
    config.num_wal_shards = 8;
    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    filemgr_open_result result = filemgr_open(fname, get_filemgr_ops(), &config, NULL);
    file = result.file;
    btreeblk_init(&btree_handle, file, nodesize);

    btree_init(&btree, (void*)&btree_handle, btreeblk_get_ops(),
               btree_kv_get_ku64_vu64(), nodesize, ksize, vsize, 0x0, NULL);

    for (i=0;i<6;++i) {
        k = i; v = i*10;
        btree_insert(&btree, (void*)&k, (void*)&v);
    }

    btree_print_node(&btree, print_btree);
    //btree_operation_end(&btree);

    for (i=6;i<12;++i) {
        k = i; v = i*10;
        btree_insert(&btree, (void*)&k, (void*)&v);
    }

    btree_print_node(&btree, print_btree);
    btreeblk_end(&btree_handle);
    //btree_operation_end(&btree);

    k = 4;
    v = 44;
    btree_insert(&btree, (void*)&k, (void*)&v);
    btree_print_node(&btree, print_btree);
    //btree_operation_end(&btree);

    btreeblk_end(&btree_handle);
    filemgr_commit(file, true, NULL);

    k = 5;
    v = 55;
    btree_insert(&btree, (void*)&k, (void*)&v);
    btree_print_node(&btree, print_btree);
    //btree_operation_end(&btree);

    btreeblk_end(&btree_handle);
    filemgr_commit(file, true, NULL);

    k = 5;
    v = 59;
    btree_insert(&btree, (void*)&k, (void*)&v);
    btree_print_node(&btree, print_btree);
    //btree_operation_end(&btree);

    btreeblk_end(&btree_handle);
    filemgr_commit(file, true, NULL);


    struct btree btree2;

    DBG("re-read using root bid %" _F64 "\n", btree.root_bid);
    btree_init_from_bid(&btree2, (void*)&btree_handle, btreeblk_get_ops(),
                        btree_kv_get_ku64_vu64(), nodesize, btree.root_bid);
    btree_print_node(&btree2, print_btree);

    btreeblk_free(&btree_handle);

    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    TEST_RESULT("basic test");
}

void iterator_test()
{
    TEST_INIT();

    int ksize = 8;
    int vsize = 8;
    int nodesize = (ksize + vsize)*4 + sizeof(struct bnode);
    int blocksize = nodesize * 2;
    struct filemgr *file;
    struct btreeblk_handle btree_handle;
    struct btree btree;
    struct btree_iterator bi;
    struct filemgr_config config;
    btree_result br;
    int i, r;
    uint64_t k,v;
    char *fname = (char *) "./btreeblock_testfile";

    memset(&config, 0, sizeof(config));
    config.blocksize = blocksize;
    config.ncacheblock = 0;
    config.flag = 0x0;
    config.options = FILEMGR_CREATE;
    config.num_wal_shards = 8;
    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    filemgr_open_result result = filemgr_open(fname, get_filemgr_ops(), &config, NULL);
    file = result.file;
    btreeblk_init(&btree_handle, file, nodesize);

    btree_init(&btree, (void*)&btree_handle, btreeblk_get_ops(),
               btree_kv_get_ku64_vu64(), nodesize, ksize, vsize, 0x0, NULL);

    for (i=0;i<6;++i) {
        k = i*2; v = i*10;
        btree_insert(&btree, (void*)&k, (void*)&v);
    }

    btree_print_node(&btree, print_btree);
    //btree_operation_end(&btree);

    for (i=6;i<12;++i) {
        k = i*2; v = i*10;
        btree_insert(&btree, (void*)&k, (void*)&v);
    }

    btree_print_node(&btree, print_btree);
    btreeblk_end(&btree_handle);
    //btree_operation_end(&btree);

    filemgr_commit(file, true, NULL);

    k = 4;
    btree_iterator_init(&btree, &bi, (void*)&k);
    for (i=0;i<3;++i){
        btree_next(&bi, (void*)&k, (void*)&v);
        DBG("%" _F64 " , %" _F64 "\n", k, v);
    }
    btree_iterator_free(&bi);

    DBG("\n");
    k = 7;
    btree_iterator_init(&btree, &bi, (void*)&k);
    for (i=0;i<3;++i){
        btree_next(&bi, (void*)&k, (void*)&v);
        DBG("%" _F64 " , %" _F64 "\n", k, v);
    }
    btree_iterator_free(&bi);

    DBG("\n");
    btree_iterator_init(&btree, &bi, NULL);
    for (i=0;i<30;++i){
        br = btree_next(&bi, (void*)&k, (void*)&v);
        if (br == BTREE_RESULT_FAIL) break;
        DBG("%" _F64 " , %" _F64 "\n", k, v);
    }
    btree_iterator_free(&bi);

    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    TEST_RESULT("iterator test");
}


void two_btree_test()
{
    TEST_INIT();

    int i;
    int nodesize = sizeof(struct bnode) + 16*4;
    int blocksize = nodesize * 4;
    struct filemgr *file;
    struct btreeblk_handle btreeblk_handle;
    struct btree btree_a, btree_b;
    struct filemgr_config config;
    uint64_t k,v;
    char *fname = (char *) "./btreeblock_testfile";
    int r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;

    memset(&config, 0, sizeof(config));
    config.blocksize = blocksize;
    config.ncacheblock = 1024;
    config.options = FILEMGR_CREATE;
    config.num_wal_shards = 8;
    filemgr_open_result result = filemgr_open(fname, get_filemgr_ops(), &config, NULL);
    file = result.file;
    btreeblk_init(&btreeblk_handle, file, nodesize);

    btree_init(&btree_a, (void*)&btreeblk_handle, btreeblk_get_ops(),
               btree_kv_get_ku64_vu64(), nodesize, 8, 8, 0x0, NULL);
    btree_init(&btree_b, (void*)&btreeblk_handle, btreeblk_get_ops(),
               btree_kv_get_ku64_vu64(), nodesize, 8, 8, 0x0, NULL);

    for (i=0;i<12;++i){
        k = i*2; v = k * 10;
        btree_insert(&btree_a, (void*)&k, (void*)&v);

        k = i*2 + 1; v = k*10 + 5;
        btree_insert(&btree_b, (void*)&k, (void*)&v);
    }

    filemgr_commit(file, true, NULL);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    btree_print_node(&btree_a, print_btree);
    btree_print_node(&btree_b, print_btree);

    TEST_RESULT("two btree test");
}

void range_test()
{
    TEST_INIT();

    int i, r, n=16, den=5;
    int blocksize = 512;
    struct filemgr *file;
    struct btreeblk_handle bhandle;
    struct btree btree;
    struct filemgr_config fconfig;
    uint64_t key, value, key_end;
    char *fname = (char *) "./btreeblock_testfile";

    memset(&fconfig, 0, sizeof(fconfig));
    fconfig.blocksize = blocksize;
    fconfig.ncacheblock = 0;
    fconfig.options = FILEMGR_CREATE;
    fconfig.num_wal_shards = 8;

    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    filemgr_open_result result = filemgr_open(fname, get_filemgr_ops(), &fconfig, NULL);
    file = result.file;
    btreeblk_init(&bhandle, file, blocksize);

    btree_init(&btree, (void*)&bhandle, btreeblk_get_ops(), btree_kv_get_ku64_vu64(),
               blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, NULL);

    for (i=0;i<n;++i){
        key = i;
        value = i*10;
        btree_insert(&btree, (void*)&key, (void*)&value);
        btreeblk_end(&bhandle);
    }

    for (i=0;i<den;++i){
        btree_get_key_range(&btree, i, den, (void*)&key, (void*)&key_end);
        DBG("%d %d\n", (int)key, (int)key_end);
    }

    btreeblk_free(&bhandle);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    TEST_RESULT("range test");
}

INLINE int is_subblock(bid_t subbid)
{
    uint8_t flag;
    flag = (subbid >> (8 * (sizeof(bid_t)-2))) & 0x00ff;
    return flag;
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

void subblock_test()
{
    TEST_INIT();

    int i, j, k, r, nbtrees;
    int nodesize;
    int blocksize = 4096;
    char *fname = (char *) "./btreeblock_testfile";
    char keybuf[256], valuebuf[256], temp[256];
    filemgr_open_result result;
    btree_result br;
    bid_t bid;
    size_t subblock_no, idx;
    struct filemgr *file;
    struct btreeblk_handle bhandle;
    struct btree_kv_ops *ops;
    struct btree btree, btree_arr[64];
    struct filemgr_config fconfig;
    struct btree_meta meta;

    memset(&fconfig, 0, sizeof(fconfig));
    fconfig.blocksize = blocksize;
    fconfig.ncacheblock = 0;
    fconfig.options = FILEMGR_CREATE;
    fconfig.num_wal_shards = 8;
    ops = btree_kv_get_kb64_vb64(NULL);

    // btree initialization using large metadata test
    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    result = filemgr_open(fname, get_filemgr_ops(), &fconfig, NULL);
    file = result.file;
    meta.data = (void*)malloc(4096);

    meta.size = 120;
    btreeblk_init(&bhandle, file, blocksize);
    btree_init(&btree, (void*)&bhandle, btreeblk_get_ops(), ops,
               blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, &meta);
    TEST_CHK(is_subblock(btree.root_bid));
    subbid2bid(btree.root_bid, &subblock_no, &idx, &bid);
    TEST_CHK(subblock_no == 1);
    btreeblk_free(&bhandle);

    meta.size = 250;
    btreeblk_init(&bhandle, file, blocksize);
    btree_init(&btree, (void*)&bhandle, btreeblk_get_ops(), ops,
               blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, &meta);
    TEST_CHK(is_subblock(btree.root_bid));
    subbid2bid(btree.root_bid, &subblock_no, &idx, &bid);
    TEST_CHK(subblock_no == 2);
    btreeblk_free(&bhandle);

    meta.size = 510;
    btreeblk_init(&bhandle, file, blocksize);
    btree_init(&btree, (void*)&bhandle, btreeblk_get_ops(), ops,
               blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, &meta);
    TEST_CHK(is_subblock(btree.root_bid));
    subbid2bid(btree.root_bid, &subblock_no, &idx, &bid);
    TEST_CHK(subblock_no == 3);
    btreeblk_free(&bhandle);

    meta.size = 1020;
    btreeblk_init(&bhandle, file, blocksize);
    btree_init(&btree, (void*)&bhandle, btreeblk_get_ops(), ops,
               blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, &meta);
    TEST_CHK(is_subblock(btree.root_bid));
    subbid2bid(btree.root_bid, &subblock_no, &idx, &bid);
    TEST_CHK(subblock_no == 4);
    btreeblk_free(&bhandle);

    meta.size = 2040;
    btreeblk_init(&bhandle, file, blocksize);
    btree_init(&btree, (void*)&bhandle, btreeblk_get_ops(), ops,
               blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, &meta);
    TEST_CHK(!is_subblock(btree.root_bid));
    btreeblk_free(&bhandle);

    meta.size = 4090;
    btreeblk_init(&bhandle, file, blocksize);
    br = btree_init(&btree, (void*)&bhandle, btreeblk_get_ops(), ops,
               blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, &meta);
    TEST_CHK(br == BTREE_RESULT_FAIL);

    btreeblk_free(&bhandle);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();
    free(meta.data);

    // coverage: enlarge case 1-1
    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    result = filemgr_open(fname, get_filemgr_ops(), &fconfig, NULL);
    file = result.file;
    btreeblk_init(&bhandle, file, blocksize);
    btree_init(&btree, (void*)&bhandle, btreeblk_get_ops(), ops,
               blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, NULL);
    for (i=0;i<256;++i){
        sprintf(keybuf, "%08d", i);
        sprintf(valuebuf, "%08x", i);
        btree_insert(&btree, (void*)keybuf, (void*)valuebuf);
        btreeblk_end(&bhandle);
        for (j=0;j<=i;++j){
            sprintf(keybuf, "%08d", j);
            sprintf(valuebuf, "%08x", j);
            btree_find(&btree, (void*)keybuf, (void*)temp);
            btreeblk_end(&bhandle);
            TEST_CHK(!memcmp(valuebuf, temp, strlen(valuebuf)));
        }
    }
    btreeblk_free(&bhandle);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    // coverage: enlarge case 1-2, move case 1
    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    result = filemgr_open(fname, get_filemgr_ops(), &fconfig, NULL);
    file = result.file;
    btreeblk_init(&bhandle, file, blocksize);
    btree_init(&btree, (void*)&bhandle, btreeblk_get_ops(), ops,
               blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, NULL);
    for (i=0;i<256;++i){
        sprintf(keybuf, "%08d", i);
        sprintf(valuebuf, "%08x", i);
        btree_insert(&btree, (void*)keybuf, (void*)valuebuf);
        btreeblk_end(&bhandle);
        filemgr_commit(file, true, NULL);
        for (j=0;j<=i;++j){
            sprintf(keybuf, "%08d", j);
            sprintf(valuebuf, "%08x", j);
            btree_find(&btree, (void*)keybuf, (void*)temp);
            btreeblk_end(&bhandle);
            TEST_CHK(!memcmp(valuebuf, temp, strlen(valuebuf)));
        }
    }
    btreeblk_free(&bhandle);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    // coverage: enlarge case 1-1, 2-1, 2-2, 3-1
    nbtrees = 2;
    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    result = filemgr_open(fname, get_filemgr_ops(), &fconfig, NULL);
    file = result.file;
    btreeblk_init(&bhandle, file, blocksize);
    for (i=0;i<nbtrees;++i){
        btree_init(&btree_arr[i], (void*)&bhandle, btreeblk_get_ops(), ops,
                   blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, NULL);
    }
    for (i=0;i<256;++i){
        for (j=0;j<nbtrees;++j){
            sprintf(keybuf, "%02d%06d", j, i);
            sprintf(valuebuf, "%02d%06x", j, i);
            btree_insert(&btree_arr[j], (void*)keybuf, (void*)valuebuf);
            btreeblk_end(&bhandle);
            for (k=0;k<=i;++k){
                sprintf(keybuf, "%02d%06d", j, k);
                sprintf(valuebuf, "%02d%06x", j, k);
                btree_find(&btree_arr[j], (void*)keybuf, (void*)temp);
                btreeblk_end(&bhandle);
                TEST_CHK(!memcmp(valuebuf, temp, strlen(valuebuf)));
            }
        }
    }
    btreeblk_free(&bhandle);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    // coverage: enlarge case 1-2, 2-1, 3-1, move case 1, 2-1, 2-2
    nbtrees = 2;
    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    result = filemgr_open(fname, get_filemgr_ops(), &fconfig, NULL);
    file = result.file;
    btreeblk_init(&bhandle, file, blocksize);
    for (i=0;i<nbtrees;++i){
        btree_init(&btree_arr[i], (void*)&bhandle, btreeblk_get_ops(), ops,
                   blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, NULL);
    }
    for (i=0;i<256;++i){
        for (j=0;j<nbtrees;++j){
            sprintf(keybuf, "%02d%06d", j, i);
            sprintf(valuebuf, "%02d%06x", j, i);
            btree_insert(&btree_arr[j], (void*)keybuf, (void*)valuebuf);
            btreeblk_end(&bhandle);
        }
        filemgr_commit(file, true, NULL);
        for (j=0;j<nbtrees;++j){
            for (k=0;k<=i;++k){
                sprintf(keybuf, "%02d%06d", j, k);
                sprintf(valuebuf, "%02d%06x", j, k);
                btree_find(&btree_arr[j], (void*)keybuf, (void*)temp);
                btreeblk_end(&bhandle);
                TEST_CHK(!memcmp(valuebuf, temp, strlen(valuebuf)));
            }
        }
    }
    btreeblk_free(&bhandle);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    // coverage: enlarge case 1-1, 2-1, 3-2, move case 1, 2-1
    nbtrees = 7;
    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    result = filemgr_open(fname, get_filemgr_ops(), &fconfig, NULL);
    file = result.file;
    btreeblk_init(&bhandle, file, blocksize);
    for (i=0;i<nbtrees;++i){
        btree_init(&btree_arr[i], (void*)&bhandle, btreeblk_get_ops(), ops,
                   blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, NULL);
    }
    for (j=0;j<nbtrees;++j){
        nodesize = 128 * (1<<j);
        for (i=0;i<(nodesize-16)/16;++i){
            sprintf(keybuf, "%02d%06d", j, i);
            sprintf(valuebuf, "%02d%06x", j, i);
            btree_insert(&btree_arr[j], (void*)keybuf, (void*)valuebuf);
            btreeblk_end(&bhandle);
        }
        filemgr_commit(file, true, NULL);
    }
    btreeblk_free(&bhandle);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    // coverage: enlarge case 1-1, 1-2, 2-1, 3-2, move case 1, 2-1
    nbtrees = 7;
    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;
    result = filemgr_open(fname, get_filemgr_ops(), &fconfig, NULL);
    file = result.file;
    btreeblk_init(&bhandle, file, blocksize);
    for (i=0;i<nbtrees;++i){
        btree_init(&btree_arr[i], (void*)&bhandle, btreeblk_get_ops(), ops,
                   blocksize, sizeof(uint64_t), sizeof(uint64_t), 0x0, NULL);
    }
    for (j=0;j<nbtrees;++j){
        nodesize = 128 * (1<<j);
        for (i=0;i<(nodesize-16)/16;++i){
            sprintf(keybuf, "%02d%06d", j, i);
            sprintf(valuebuf, "%02d%06x", j, i);
            btree_insert(&btree_arr[j], (void*)keybuf, (void*)valuebuf);
            btreeblk_end(&bhandle);
            filemgr_commit(file, true, NULL);
        }
    }
    btreeblk_free(&bhandle);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    free(ops);
    TEST_RESULT("subblock test");
}

void btree_reverse_iterator_test()
{
    TEST_INIT();

    int ksize = 8, vsize = 8, r, c;
    int nodesize = 256;
    struct filemgr *file;
    struct btreeblk_handle bhandle;
    struct btree btree;
    struct btree_iterator bi;
    struct filemgr_config config;
    struct btree_kv_ops *kv_ops;
    btree_result br;
    filemgr_open_result fr;
    uint64_t i;
    uint64_t k,v;
    char *fname = (char *) "./btreeblock_testfile";

    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;

    memleak_start();

    memset(&config, 0, sizeof(config));
    config.blocksize = nodesize;
    config.options = FILEMGR_CREATE;
    config.num_wal_shards = 8;
    fr = filemgr_open(fname, get_filemgr_ops(), &config, NULL);
    file = fr.file;

    btreeblk_init(&bhandle, file, nodesize);
    kv_ops = btree_kv_get_kb64_vb64(NULL);
    btree_init(&btree, (void*)&bhandle,
               btreeblk_get_ops(), kv_ops,
               nodesize, ksize, vsize, 0x0, NULL);

    for (i=10;i<40;++i) {
        k = _endian_encode(i*0x10);
        v = _endian_encode(i*0x100);
        btree_insert(&btree, (void*)&k, (void*)&v);
        btreeblk_end(&bhandle);
    }

    c = 0;
    btree_iterator_init(&btree, &bi, NULL);
    while ((br=btree_next(&bi, &k, &v)) == BTREE_RESULT_SUCCESS) {
        btreeblk_end(&bhandle);
        k = _endian_decode(k);
        v = _endian_decode(v);
        TEST_CHK(k == (uint64_t)(c+10)*0x10);
        TEST_CHK(v == (uint64_t)(c+10)*0x100);
        c++;
    }
    btreeblk_end(&bhandle);
    btree_iterator_free(&bi);
    TEST_CHK(c == 30);

    c = 0;
    i=10000;
    k = _endian_encode(i);
    btree_iterator_init(&btree, &bi, &k);
    while ((br=btree_next(&bi, &k, &v)) == BTREE_RESULT_SUCCESS) {
        btreeblk_end(&bhandle);
        k = _endian_decode(k);
        v = _endian_decode(v);
    }
    btreeblk_end(&bhandle);
    btree_iterator_free(&bi);
    TEST_CHK(c == 0);

    // reverse iteration with NULL initial key
    c = 0;
    btree_iterator_init(&btree, &bi, NULL);
    while ((br=btree_prev(&bi, &k, &v)) == BTREE_RESULT_SUCCESS) {
        btreeblk_end(&bhandle);
        k = _endian_decode(k);
        v = _endian_decode(v);
    }
    btreeblk_end(&bhandle);
    btree_iterator_free(&bi);
    TEST_CHK(c == 0);

    c = 0;
    i=10000;
    k = _endian_encode(i);
    btree_iterator_init(&btree, &bi, &k);
    while ((br=btree_prev(&bi, &k, &v)) == BTREE_RESULT_SUCCESS) {
        btreeblk_end(&bhandle);
        k = _endian_decode(k);
        v = _endian_decode(v);
        TEST_CHK(k == (uint64_t)(39-c)*0x10);
        TEST_CHK(v == (uint64_t)(39-c)*0x100);
        c++;
    }
    btreeblk_end(&bhandle);
    btree_iterator_free(&bi);
    TEST_CHK(c == 30);

    c = 0;
    i=0x175;
    k = _endian_encode(i);
    btree_iterator_init(&btree, &bi, &k);
    while ((br=btree_prev(&bi, &k, &v)) == BTREE_RESULT_SUCCESS) {
        btreeblk_end(&bhandle);
        k = _endian_decode(k);
        v = _endian_decode(v);
        TEST_CHK(k == (uint64_t)(0x17-c)*0x10);
        TEST_CHK(v == (uint64_t)(0x17-c)*0x100);
        c++;
    }
    btreeblk_end(&bhandle);
    btree_iterator_free(&bi);
    TEST_CHK(c == 14);

    c = 0xa0 - 0x10;
    btree_iterator_init(&btree, &bi, NULL);
    for (i=0;i<15;++i){
        c += 0x10;
        br = btree_next(&bi, &k, &v);
        TEST_CHK(br == BTREE_RESULT_SUCCESS);
        btreeblk_end(&bhandle);
        k = _endian_decode(k);
        v = _endian_decode(v);
        TEST_CHK(k == (uint64_t)c);
        TEST_CHK(v == (uint64_t)c*0x10);
    }
    for (i=0;i<7;++i){
        c -= 0x10;
        br = btree_prev(&bi, &k, &v);
        TEST_CHK(br == BTREE_RESULT_SUCCESS);
        btreeblk_end(&bhandle);
        k = _endian_decode(k);
        v = _endian_decode(v);
        TEST_CHK(k == (uint64_t)c);
        TEST_CHK(v == (uint64_t)c*0x10);
    }
    for (i=0;i<10;++i){
        c += 0x10;
        br = btree_next(&bi, &k, &v);
        TEST_CHK(br == BTREE_RESULT_SUCCESS);
        btreeblk_end(&bhandle);
        k = _endian_decode(k);
        v = _endian_decode(v);
        TEST_CHK(k == (uint64_t)c);
        TEST_CHK(v == (uint64_t)c*0x10);
    }
    for (i=0;i<17;++i){
        c -= 0x10;
        br = btree_prev(&bi, &k, &v);
        TEST_CHK(br == BTREE_RESULT_SUCCESS);
        btreeblk_end(&bhandle);
        k = _endian_decode(k);
        v = _endian_decode(v);
        TEST_CHK(k == (uint64_t)c);
        TEST_CHK(v == (uint64_t)c*0x10);
    }
    br = btree_prev(&bi, &k, &v);
    btreeblk_end(&bhandle);
    TEST_CHK(br == BTREE_RESULT_FAIL);

    btree_iterator_free(&bi);

    free(kv_ops);
    btreeblk_free(&bhandle);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    memleak_end();

    TEST_RESULT("btree reverse iterator test");
}

int load_test_next_kv(void** key_out, void** val_out, void* aux) {
    uint64_t* idx = (uint64_t*)aux;
    static char kk[16];
    static char vv[16];
    sprintf(kk, "k%07lu", *idx);
    sprintf(vv, "v%07lu", *idx);
    (*idx)++;
    *key_out = (void*)kk;
    *val_out = (void*)vv;
    return 0;
}

void load_test_write_done(void* voidhandle, bid_t bid, void* aux) {
    btreeblk_write_done(voidhandle, bid);
}

#include <sys/time.h>

void btree_initial_load_test(uint64_t num_entries)
{
    TEST_INIT();

    int ksize = 8, vsize = 8, r;
    int nodesize = 4096;
    struct filemgr *file;
    struct btreeblk_handle bhandle;
    struct btree btree;
    struct filemgr_config f_config;
    struct btree_kv_ops *kv_ops;
    filemgr_open_result fr;
    char *fname = (char *) "./btreeblock_testfile";

    r = system(SHELL_DEL" btreeblock_testfile");
    (void)r;

    memleak_start();

    memset(&f_config, 0, sizeof(f_config));
    f_config.blocksize = nodesize;
    f_config.options = FILEMGR_CREATE;
    f_config.num_wal_shards = 8;
    fr = filemgr_open(fname, get_filemgr_ops(), &f_config, NULL);
    file = fr.file;

    struct timeval st, et;
    gettimeofday(&st, NULL);

    btreeblk_init(&bhandle, file, nodesize);
    kv_ops = btree_kv_get_kb64_vb64(NULL);

    uint64_t idx = 0;

#if 1
    btree_init_and_load(&btree, (void*)&bhandle,
                        btreeblk_get_ops(), kv_ops,
                        nodesize, ksize, vsize, 0x0, NULL,
                        num_entries, load_test_next_kv, load_test_write_done, &idx);
    btreeblk_end(&bhandle);
#else
    btree_init(&btree, (void*)&bhandle,
               btreeblk_get_ops(), kv_ops,
               nodesize, ksize, vsize, 0x0, NULL);
    for (size_t ii = 0; ii < num_entries; ++ii) {
        char key[16];
        char val[16];
        sprintf(key, "k%07zu", ii);
        sprintf(val, "v%07zu", ii);
        btree_insert(&btree, key, val);
        btreeblk_end(&bhandle);
    }

#endif
    gettimeofday(&et, NULL);
    btreeblk_free(&bhandle);

    {
        uint64_t ust = st.tv_sec * 1000000UL + st.tv_usec;
        uint64_t uet = et.tv_sec * 1000000UL + et.tv_usec;
        printf("%lu us elapsed, %.1f ops/s\n",
               uet - ust,
               (double)num_entries / (uet - ust) * 1000000.0);
    }

    gettimeofday(&st, NULL);

    struct btree btree_verify;
    struct btreeblk_handle bhandle_verify;
    btreeblk_init(&bhandle_verify, file, nodesize);
    btree_init_from_bid(&btree_verify, &bhandle_verify, btreeblk_get_ops(), kv_ops,
                        nodesize, btree.root_bid);
    for (size_t ii = 0; ii < num_entries; ++ii) {
        char key[16];
        char val[16];
        char val_verify[16];
        sprintf(key, "k%07zu", ii);
        sprintf(val_verify, "v%07zu", ii);
        btree_find(&btree_verify, key, val);
        btreeblk_end(&bhandle_verify);
        TEST_CHK(memcmp(val, val_verify, 8) == 0);
    }
    btreeblk_free(&bhandle_verify);

    gettimeofday(&et, NULL);
    {
        uint64_t ust = st.tv_sec * 1000000UL + st.tv_usec;
        uint64_t uet = et.tv_sec * 1000000UL + et.tv_usec;
        printf("%lu us elapsed, %.1f ops/s\n",
               uet - ust,
               (double)num_entries / (uet - ust) * 1000000.0);
    }

    free(kv_ops);
    filemgr_close(file, true, NULL, NULL);
    filemgr_shutdown();

    memleak_end();

    TEST_RESULT("btree init and load test");
}

int main()
{
#ifdef _MEMPOOL
    mempool_init();
#endif

    basic_test();
    iterator_test();
    two_btree_test();
    range_test();
    subblock_test();
    btree_reverse_iterator_test();
    btree_initial_load_test(10);
    btree_initial_load_test(100000);

    return 0;
}
