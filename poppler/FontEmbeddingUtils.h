//========================================================================
//
// FontEmbeddingUtils.h
//
// Copyright (C) 2022 Georgiy Sgibnev <georgiy@sgibnev.com>. Work sponsored by lab50.net.
//
// This file is licensed under the GPLv2 or later
//
//========================================================================

#ifndef FONT_EMBEDDING_UTILS_H
#define FONT_EMBEDDING_UTILS_H

#include <string>

#include "poppler_private_export.h"

class GooFile;
class GooString;
struct Ref;
class XRef;

namespace FontEmbeddingUtils {

// Inserts a new global font resource (an object of type "Font" referred to in a resource dictionary).
// Args:
//     xref: Document's xref.
//     fontFile: A font file to embed.
// Returns ref to a new object or Ref::INVALID.
Ref POPPLER_PRIVATE_EXPORT embed(XRef *xref, const GooFile &fontFile);

// Same as above, but fontPath is a path to a font file.
Ref POPPLER_PRIVATE_EXPORT embed(XRef *xref, const std::string &fontPath);

}

#endif
