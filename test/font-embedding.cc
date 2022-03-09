//========================================================================
//
// font-embedding.cc
// A test util to check FontEmbeddingUtils::embed().
//
// This file is licensed under the GPLv2 or later
//
// Copyright (C) 2022 Georgiy Sgibnev <georgiy@sgibnev.com>. Work sponsored by lab50.net.
//
//========================================================================

#include <config.h>
#include <cstdio>
#include <string>

#include "utils/parseargs.h"
#include "goo/GooString.h"
#include "Object.h"
#include "Dict.h"
#include "PDFDoc.h"
#include "PDFDocFactory.h"
#include "FontEmbeddingUtils.h"

static bool fail = false;
static bool printHelp = false;

static const ArgDesc argDesc[] = { { "-fail", argFlag, &fail, 0, "the font embedding API is expected to fail" },
                                   { "-h", argFlag, &printHelp, 0, "print usage information" },
                                   { "-help", argFlag, &printHelp, 0, "print usage information" },
                                   { "--help", argFlag, &printHelp, 0, "print usage information" },
                                   { "-?", argFlag, &printHelp, 0, "print usage information" },
                                   {} };

int main(int argc, char *argv[])
{
    // Parse args.
    const bool ok = parseArgs(argDesc, &argc, argv);
    if (!ok || (argc != 3) || printHelp) {
        printUsage(argv[0], "PDF-FILE FONT-FILE", argDesc);
        return (printHelp) ? 0 : 1;
    }
    const GooString docPath(argv[1]);
    const GooString fontPath(argv[2]);

    auto doc = std::unique_ptr<PDFDoc>(PDFDocFactory().createPDFDoc(docPath));
    if (!doc->isOk()) {
        fprintf(stderr, "Error opening input PDF file.\n");
        return 1;
    }

    // Embed a font.
    Ref baseFontRef = FontEmbeddingUtils::embed(doc->getXRef(), fontPath.toStr());
    if (baseFontRef == Ref::INVALID()) {
        if (fail) {
            return 0;
        } else {
            fprintf(stderr, "FontEmbeddingUtils::embed() failed.\n");
            return 1;
        }
    }

    // Check the font object.
    Object baseFontObj = Object(baseFontRef).fetch(doc->getXRef());
    Dict *baseFontDict = baseFontObj.getDict();
    if (std::string("Font") != baseFontDict->lookup("Type").getName()) {
        fprintf(stderr, "A problem with Type.\n");
        return 1;
    }
    return 0;
}
