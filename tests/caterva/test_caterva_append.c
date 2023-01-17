/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include "test_common.h"

typedef struct {
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    int64_t buffershape[CATERVA_MAX_DIM];
    int8_t axis;
} test_shapes_t;


CUTEST_TEST_DATA(append) {
    blosc2_storage *b_storage;
};


CUTEST_TEST_SETUP(append) {
    blosc2_init();

    // Add parametrizations
    CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(
            1,
            2,
            4,
            8,
    ));

    CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
            {false, false},
            {true, false},
            {true, true},
            {false, true},
    ));


    CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
            {1, {5}, {3}, {2}, {10}, 0},
            {2, {18, 6}, {6, 6}, {3, 3}, {18, 12}, 1},
            {3, {12, 10, 14}, {3, 5, 9}, {3, 4, 4}, {12, 10, 18}, 2},
            {4, {10, 10, 5, 5}, {5, 7, 3, 3}, {2, 2, 1, 1}, {10, 10, 5, 30}, 3},

    ));
}

CUTEST_TEST_TEST(append) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, test_shapes_t);
    CUTEST_GET_PARAMETER(typesize, uint8_t);

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    cparams.nthreads = 2;
    cparams.compcode = BLOSC_BLOSCLZ;
    cparams.typesize = typesize;
    blosc2_storage b_storage = {.cparams=&cparams, .dparams=&dparams};
    data->b_storage = &b_storage;

    char *urlpath = "test_append_shape.b2frame";
    blosc2_remove_urlpath(urlpath);

    caterva_params_t params;
    params.ndim = shapes.ndim;
    for (int i = 0; i < params.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    caterva_storage_t storage = {.b_storage=data->b_storage};
    if (backend.persistent) {
        storage.b_storage->urlpath = urlpath;
    }
    storage.b_storage->contiguous = backend.contiguous;
    for (int i = 0; i < params.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
    }
    int32_t blocknitems = 1;
    for (int i = 0; i < params.ndim; ++i) {
      blocknitems *= storage.blockshape[i];
    }
    storage.b_storage->cparams->blocksize = blocknitems * storage.b_storage->cparams->typesize;
    blosc2_context *ctx = blosc2_create_cctx(*storage.b_storage->cparams);

    int64_t buffersize = typesize;
    for (int i = 0; i < params.ndim; ++i) {
        buffersize *= shapes.buffershape[i];
    }

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    uint8_t *value = malloc(typesize);
    int8_t fill_value = 1;
    switch (typesize) {
        case 8:
            ((int64_t *) value)[0] = (int64_t) fill_value;
            break;
        case 4:
            ((int32_t *) value)[0] = (int32_t) fill_value;
            break;
        case 2:
            ((int16_t *) value)[0] = (int16_t) fill_value;
            break;
        case 1:
            ((int8_t *) value)[0] = fill_value;
            break;
        default:
            break;
    }
    CATERVA_ERROR(caterva_full(&params, &storage, value, &src));

    uint8_t *buffer = malloc(buffersize);
    fill_buf(buffer, typesize, buffersize / typesize);
    CATERVA_ERROR(caterva_append(ctx, src, buffer, buffersize, shapes.axis));

    int64_t start[CATERVA_MAX_DIM] = {0};
    start[shapes.axis] = shapes.shape[shapes.axis];
    int64_t stop[CATERVA_MAX_DIM];
    for (int i = 0; i < shapes.ndim; ++i) {
        stop[i] = shapes.shape[i];
    }
    stop[shapes.axis] = shapes.shape[shapes.axis] + shapes.buffershape[shapes.axis];

    /* Fill buffer with a slice from the new chunks */
    uint8_t *res_buffer = malloc(buffersize);
    CATERVA_ERROR(caterva_get_slice_buffer(ctx, src, start, stop, res_buffer,
                                           shapes.buffershape, buffersize));

    for (uint64_t i = 0; i < (uint64_t) buffersize / typesize; ++i) {
        switch (typesize) {
            case 8:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint64_t *) buffer)[i] == ((uint64_t *) res_buffer)[i]);
                break;
            case 4:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint32_t *) buffer)[i] == ((uint32_t *) res_buffer)[i]);
                break;
            case 2:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint16_t *) buffer)[i] == ((uint16_t *) res_buffer)[i]);
                break;
            case 1:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint8_t *) buffer)[i] == ((uint8_t *) res_buffer)[i]);
                break;
            default:
                CATERVA_TEST_ASSERT(CATERVA_ERR_INVALID_ARGUMENT);
        }
    }
    /* Free mallocs */
    free(value);
    free(buffer);
    free(res_buffer);

    CATERVA_TEST_ASSERT(caterva_free(&src));
    blosc2_free_ctx(ctx);
    blosc2_remove_urlpath(urlpath);

    return 0;
}

CUTEST_TEST_TEARDOWN(append) {
    blosc2_destroy();
}

int main() {
    CUTEST_TEST_RUN(append);
}