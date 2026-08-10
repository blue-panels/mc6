/*
   src/panel-plugins/arcmc - tests for libarchive stream reader

   Copyright (C) 2026
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/src/panel-plugins/arcmc"

#include "tests/mctest.h"

#include <archive.h>
#include <archive_entry.h>
#include <string.h>

#include "src/panel-plugins/arcmc/arcmc-reader.h"

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    mc_pp_input_stream_t base;
    GBytes *bytes;
    gsize offset;
    guint open_count;
    guint close_count;
} test_input_stream_t;

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
test_input_stream_open (mc_pp_input_stream_t *stream, void **handle, GError **error)
{
    test_input_stream_t *test_stream = (test_input_stream_t *) stream;

    (void) error;

    test_stream->offset = 0;
    test_stream->open_count++;
    *handle = test_stream;
    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static gssize
test_input_stream_read (mc_pp_input_stream_t *stream, void *handle, void *buf, gsize size,
                        GError **error)
{
    test_input_stream_t *test_stream = (test_input_stream_t *) stream;
    gsize length;
    const void *data;
    gsize bytes;

    (void) handle;
    (void) error;

    data = g_bytes_get_data (test_stream->bytes, &length);
    if (test_stream->offset == length)
        return 0;

    bytes = MIN (MIN (size, (gsize) 17), length - test_stream->offset);
    memcpy (buf, (const char *) data + test_stream->offset, bytes);
    test_stream->offset += bytes;
    return (gssize) bytes;
}

/* --------------------------------------------------------------------------------------------- */

static void
test_input_stream_close (mc_pp_input_stream_t *stream, void *handle)
{
    test_input_stream_t *test_stream = (test_input_stream_t *) stream;

    (void) handle;

    test_stream->close_count++;
}

/* --------------------------------------------------------------------------------------------- */

static void
test_input_stream_free (mc_pp_input_stream_t *stream)
{
    test_input_stream_t *test_stream = (test_input_stream_t *) stream;

    g_bytes_unref (test_stream->bytes);
    g_free (test_stream);
}

/* --------------------------------------------------------------------------------------------- */

static gint64
test_input_stream_seek (mc_pp_input_stream_t *stream, void *handle, gint64 offset, int whence,
                        GError **error)
{
    test_input_stream_t *test_stream = (test_input_stream_t *) stream;
    gint64 target;
    gsize length;

    (void) handle;
    (void) error;

    g_bytes_get_data (test_stream->bytes, &length);

    switch (whence)
    {
    case SEEK_SET:
        target = offset;
        break;
    case SEEK_CUR:
        target = (gint64) test_stream->offset + offset;
        break;
    case SEEK_END:
        target = (gint64) length + offset;
        break;
    default:
        return -1;
    }

    if (target < 0 || target > (gint64) length)
        return -1;

    test_stream->offset = (gsize) target;
    return target;
}

/* --------------------------------------------------------------------------------------------- */

static const mc_pp_input_stream_ops_t test_input_stream_ops = {
    .open = test_input_stream_open,
    .read = test_input_stream_read,
    .close = test_input_stream_close,
    .free = test_input_stream_free,
};

/* --------------------------------------------------------------------------------------------- */

static const mc_pp_input_stream_ops_t test_seekable_input_stream_ops = {
    .open = test_input_stream_open,
    .read = test_input_stream_read,
    .seek = test_input_stream_seek,
    .close = test_input_stream_close,
    .free = test_input_stream_free,
};

/* --------------------------------------------------------------------------------------------- */

static test_input_stream_t *
test_input_stream_new (void)
{
    struct archive *writer;
    struct archive_entry *entry;
    test_input_stream_t *stream;
    char *buffer;
    size_t used;
    static const char contents[] = "streamed contents";

    buffer = g_malloc (64 * 1024);
    writer = archive_write_new ();
    archive_write_set_format_ustar (writer);
    ck_assert_int_eq (archive_write_open_memory (writer, buffer, 64 * 1024, &used), ARCHIVE_OK);

    entry = archive_entry_new ();
    archive_entry_set_pathname (entry, "inside.txt");
    archive_entry_set_filetype (entry, AE_IFREG);
    archive_entry_set_perm (entry, 0644);
    archive_entry_set_size (entry, sizeof (contents) - 1);
    ck_assert_int_eq (archive_write_header (writer, entry), ARCHIVE_OK);
    ck_assert_int_eq (archive_write_data (writer, contents, sizeof (contents) - 1),
                      sizeof (contents) - 1);
    archive_entry_free (entry);
    ck_assert_int_eq (archive_write_close (writer), ARCHIVE_OK);
    archive_write_free (writer);

    stream = g_new0 (test_input_stream_t, 1);
    stream->base.ops = &test_input_stream_ops;
    stream->bytes = g_bytes_new_take (buffer, used);
    return stream;
}

/* --------------------------------------------------------------------------------------------- */

/* 7z keeps its directory at the end of the file, so it stands for every format
   that cannot be read in a single pass. */
static test_input_stream_t *
test_7z_input_stream_new (gboolean seekable)
{
    struct archive *writer;
    struct archive_entry *entry;
    test_input_stream_t *stream;
    char *buffer;
    char *payload;
    size_t used;
    gsize payload_size = 256 * 1024;
    gsize i;
    int k;

    payload = g_malloc (payload_size);
    for (i = 0; i < payload_size; i++)
        payload[i] = (char) (i * 2654435761u >> 11);

    buffer = g_malloc (4 * 1024 * 1024);
    writer = archive_write_new ();
    ck_assert_int_eq (archive_write_set_format_7zip (writer), ARCHIVE_OK);
    ck_assert_int_eq (archive_write_open_memory (writer, buffer, 4 * 1024 * 1024, &used),
                      ARCHIVE_OK);

    for (k = 0; k < 8; k++)
    {
        char name[32];

        g_snprintf (name, sizeof (name), "file%d.bin", k);
        entry = archive_entry_new ();
        archive_entry_set_pathname (entry, name);
        archive_entry_set_filetype (entry, AE_IFREG);
        archive_entry_set_perm (entry, 0644);
        archive_entry_set_size (entry, (la_int64_t) payload_size);
        ck_assert_int_eq (archive_write_header (writer, entry), ARCHIVE_OK);
        ck_assert_int_eq (archive_write_data (writer, payload, payload_size),
                          (la_ssize_t) payload_size);
        archive_entry_free (entry);
    }

    ck_assert_int_eq (archive_write_close (writer), ARCHIVE_OK);
    archive_write_free (writer);
    g_free (payload);

    stream = g_new0 (test_input_stream_t, 1);
    stream->base.ops = seekable ? &test_seekable_input_stream_ops : &test_input_stream_ops;
    stream->bytes = g_bytes_new_take (buffer, used);
    return stream;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reads_tar_from_chunked_input_stream)
{
    test_input_stream_t *stream;
    arcmc_data_t data = { 0 };
    arcmc_archive_reader_ctx_t *ctx;
    struct archive *reader;
    struct archive_entry *entry;
    char contents[32] = { 0 };

    stream = test_input_stream_new ();
    data.archive_path = g_strdup ("remote.tar");
    data.input_stream = &stream->base;

    reader = arcmc_archive_reader_open (&data, &ctx);
    ck_assert_ptr_nonnull (reader);
    ck_assert_int_eq (archive_read_next_header (reader, &entry), ARCHIVE_OK);
    ck_assert_str_eq (archive_entry_pathname (entry), "inside.txt");
    ck_assert_int_eq (archive_read_data (reader, contents, sizeof (contents)), 17);
    ck_assert_str_eq (contents, "streamed contents");

    arcmc_archive_reader_close (reader, ctx);
    ck_assert_uint_eq (stream->open_count, 1);
    ck_assert_uint_eq (stream->close_count, 1);

    mc_pp_input_stream_free (data.input_stream);
    g_free (data.archive_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reopens_input_stream_for_each_archive_operation)
{
    test_input_stream_t *stream;
    arcmc_data_t data = { 0 };
    arcmc_archive_reader_ctx_t *ctx;
    struct archive *reader;
    struct archive_entry *entry;
    int i;

    stream = test_input_stream_new ();
    data.archive_path = g_strdup ("remote.tar");
    data.input_stream = &stream->base;

    for (i = 0; i < 2; i++)
    {
        reader = arcmc_archive_reader_open (&data, &ctx);
        ck_assert_ptr_nonnull (reader);
        ck_assert_int_eq (archive_read_next_header (reader, &entry), ARCHIVE_OK);
        arcmc_archive_reader_close (reader, ctx);
    }

    ck_assert_uint_eq (stream->open_count, 2);
    ck_assert_uint_eq (stream->close_count, 2);

    mc_pp_input_stream_free (data.input_stream);
    g_free (data.archive_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reads_7z_only_when_the_stream_can_seek)
{
    test_input_stream_t *stream;
    arcmc_data_t data = { 0 };
    arcmc_archive_reader_ctx_t *ctx;
    struct archive *reader;
    struct archive_entry *entry;
    int entries = 0;

    stream = test_7z_input_stream_new (TRUE);
    data.archive_path = g_strdup ("remote.7z");
    data.input_stream = &stream->base;

    reader = arcmc_archive_reader_open (&data, &ctx);
    mctest_assert_not_null (reader);
    while (archive_read_next_header (reader, &entry) == ARCHIVE_OK)
        entries++;
    ck_assert_int_eq (entries, 8);
    arcmc_archive_reader_close (reader, ctx);
    mc_pp_input_stream_free (data.input_stream);

    stream = test_7z_input_stream_new (FALSE);
    data.input_stream = &stream->base;

    reader = arcmc_archive_reader_open (&data, &ctx);
    if (reader != NULL)
    {
        entries = 0;
        while (archive_read_next_header (reader, &entry) == ARCHIVE_OK)
            entries++;
        ck_assert_int_eq (entries, 0);
        arcmc_archive_reader_close (reader, ctx);
    }

    mc_pp_input_stream_free (data.input_stream);
    g_free (data.archive_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_reads_tar_from_chunked_input_stream);
    tcase_add_test (tc_core, test_reopens_input_stream_for_each_archive_operation);
    tcase_add_test (tc_core, test_reads_7z_only_when_the_stream_can_seek);

    return mctest_run_all (tc_core);
}
