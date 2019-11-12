//========================================================================
//
// FlateStream.cc
//
// Copyright (C) 2005, Jeff Muizelaar <jeff@infidigm.net>
// Copyright (C) 2010, Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2016, William Bader <williambader@hotmail.com>
// Copyright (C) 2017, Adrian Johnson <ajohnson@redneon.com>
//
// This file is under the GPLv2 or later license
//
//========================================================================

#include <config.h>

#include "poppler-config.h"

#ifdef ENABLE_ZLIB_UNCOMPRESS

#include "FlateStream.h"

FlateStream::FlateStream(Stream *strA, int predictor, int columns, int colors, int bits) :
  FilterStream(strA)
{
  if (predictor != 1) {
    pred = new StreamPredictor(this, predictor, columns, colors, bits);
  } else {
    pred = NULL;
  }
  out_pos = 0;
  memset(&d_stream, 0, sizeof(d_stream));
  inflateInit(&d_stream);
}

FlateStream::~FlateStream() {
  inflateEnd(&d_stream);
  delete pred;
  delete str;
}

void FlateStream::reset() {
  //FIXME: what are the semantics of reset?
  //i.e. how much initialization has to happen in the constructor?

  /* reinitialize zlib */
  inflateEnd(&d_stream);
  memset(&d_stream, 0, sizeof(d_stream));
  inflateInit(&d_stream);

  str->reset();
  d_stream.avail_in = 0;
  status = Z_OK;
  out_pos = 0;
  out_buf_len = 0;
}

int FlateStream::getRawChar() {
  return doGetRawChar();
}

void FlateStream::getRawChars(int nChars, int *buffer) {
  for (int i = 0; i < nChars; ++i)
    buffer[i] = doGetRawChar();
}

int FlateStream::getChar() {
  if (pred)
    return pred->getChar();
  else
    return getRawChar();
}

int FlateStream::lookChar() {
  if (pred)
    return pred->lookChar();

  if (fill_buffer())
    return EOF;

  return out_buf[out_pos];
}

int FlateStream::fill_buffer() {
  /* only fill the buffer if it has all been used */
  if (out_pos >= out_buf_len) {
    /* check if the flatestream has been exhausted */
    if (status == Z_STREAM_END) {
      return -1;
    }

    /* set to the beginning of out_buf */
    d_stream.avail_out = sizeof(out_buf);
    d_stream.next_out = out_buf;
    out_pos = 0;

    while (1) {
      /* buffer is empty so we need to fill it */
      if (d_stream.avail_in == 0) {
	int c;
	/* read from the source stream */
	while (d_stream.avail_in < sizeof(in_buf) && (c = str->getChar()) != EOF) {
	  in_buf[d_stream.avail_in++] = c;
	}
	d_stream.next_in = in_buf;
      }

      /* keep decompressing until we can't anymore */
      if (d_stream.avail_out == 0 || d_stream.avail_in == 0 || (status != Z_OK && status != Z_BUF_ERROR))
	break;
      status = inflate(&d_stream, Z_SYNC_FLUSH);
    }

    out_buf_len = sizeof(out_buf) - d_stream.avail_out;
    if (status != Z_OK && status != Z_STREAM_END)
      return -1;
    if (!out_buf_len)
      return -1;
  }

  return 0;
}

GooString *FlateStream::getPSFilter(int psLevel, const char *indent) {
  GooString *s;

  if (psLevel < 3 || pred) {
    return NULL;
  }
  if (!(s = str->getPSFilter(psLevel, indent))) {
    return NULL;
  }
  s->append(indent)->append("<< >> /FlateDecode filter\n");
  return s;
}

bool FlateStream::isBinary(bool last) {
  return str->isBinary(true);
}

bool FlateStream::checkAdler32() {
  int c = 0;

  reset();
  d_stream.avail_out = sizeof(out_buf);

  // decompress all data. zlib will report an error if the checksum failed.
  while (status == Z_OK) {
    // fill input buffer
    if (d_stream.avail_in == 0 && c != EOF) {
      while (d_stream.avail_in < sizeof(in_buf) &&
             (c = str->getChar()) != EOF) {
        in_buf[d_stream.avail_in++] = c;
      }
      d_stream.next_in = in_buf;
    }

    // break if out of input data and no more to flush out from zlib
    if (d_stream.avail_in == 0 && d_stream.avail_out != 0)
      break;

    d_stream.next_out = out_buf;
    d_stream.avail_out = sizeof(out_buf);
    status = inflate(&d_stream, Z_NO_FLUSH);
  }

  close();
  return (status == Z_STREAM_END);
}

FlateStream *FlateStream::createVerified(Stream *strA, int predictor,
                                         int columns, int colors, int bits) {
  FlateStream *flateStr = new FlateStream(strA, predictor, columns, colors,
                                          bits);
  if (!flateStr->checkAdler32()) {
    flateStr->str = nullptr; // steal back stream so it isn't also deleted
    delete flateStr;
    return nullptr;
  }

  return flateStr;
}

#endif
