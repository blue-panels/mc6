/** \file arcmc-reader.h
 *  \brief Header: libarchive reader over a plugin input stream
 */

#ifndef ARCMC_READER_H
#define ARCMC_READER_H

#include "arcmc-types.h"

struct archive;

typedef struct arcmc_archive_reader_ctx arcmc_archive_reader_ctx_t;

struct archive *arcmc_archive_reader_open (const arcmc_data_t *data,
                                           arcmc_archive_reader_ctx_t **ctx);
void arcmc_archive_reader_close (struct archive *archive, arcmc_archive_reader_ctx_t *ctx);

#endif /* ARCMC_READER_H */
