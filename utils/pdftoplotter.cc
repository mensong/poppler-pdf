//========================================================================
//
// pdftoplotter.cc
//
// Render PDF using only simple straight line-segment strokes, suitable for
// drawing on a pen plotter (or other vector device). Fonts can be substituted
// with Hershey (stroke) fonts rather than drawn as outlines.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#include "config.h"
#include <poppler-config.h>
#include "parseargs.h"
#include "PDFDocFactory.h"
#include "splash/SplashBitmap.h"

#include "plotlib/StrokeOutputDev.h"

using namespace std;

struct Args
{
    bool help = false;
    char inputFilename[1024] = { '\0' };
    char outputFilename[1024] = { '\0' };
    char format[1024] = { '\0' };
    int pageNum = 1;
    int alarm = 0;
    bool verboseFonts = false;
} args;

static const ArgDesc argDesc[] = { { "-h", argFlag, &args.help, 0, "Display this help message" },
                                   { "-i", argString, &args.inputFilename, sizeof(args.inputFilename), "Input file name" },
                                   { "--input", argString, &args.inputFilename, sizeof(args.inputFilename), "Input file name" },
                                   { "-o", argString, &args.outputFilename, sizeof(args.outputFilename), "Output file name" },
                                   { "--output", argString, &args.outputFilename, sizeof(args.outputFilename), "Output file name" },
                                   { "--format", argString, &args.format, sizeof(args.format), "Format: hpgl | svg (default: guess from extn)" },
                                   { "-n", argInt, &args.pageNum, sizeof(args.pageNum), "Page number to render" },
                                   { "--verboseFonts", argFlag, &args.verboseFonts, false, "Print font substitution decisions." },
                                   { "--debugAlarm", argInt, &args.alarm, sizeof(args.alarm), "(debug) Stop rendering after n objects" },
                                   {} };

static int processPage()
{
    GooString *fileName = new GooString(args.inputFilename);
    unique_ptr<PDFDoc> document = PDFDocFactory().createPDFDoc(*fileName, nullptr, nullptr);
    if (document == nullptr || !document->isOk()) {
        return -1;
    }
    if (!document->getPage(args.pageNum)) {
        cerr << "Document has no page " << args.pageNum << endl;
        return -1;
    }

    StrokeOutputDev *strokeOut = new StrokeOutputDev(args.verboseFonts);
    strokeOut->setDebugAlarm(args.alarm);
    strokeOut->startDoc(document.get());

    int x_resolution = 150;
    int y_resolution = 150;
    document->displayPageSlice(strokeOut, args.pageNum, x_resolution, y_resolution,
                               /*rotate*/ 0,
                               /*useMediaBox*/ true,
                               /*crop*/ true,
                               /*printing*/ false, -1, -1, -1, -1,
                               /*abortCheckCbk*/ nullptr,
                               /*annotDisplayDecideCbk*/ nullptr,
                               /*annotDisplayDecideCbkData*/ nullptr);

    SplashBitmap *bitmap = strokeOut->getBitmap();
    bitmap->writeImgFile(splashFormatPng, "proof.png", x_resolution, y_resolution);

    if (strcmp(args.format, "hpgl") == 0) {
        strokeOut->writePlotHpgl(args.outputFilename);
    }
    if (strcmp(args.format, "svg") == 0) {
        strokeOut->writePlotSvg(args.outputFilename);
    }

    delete strokeOut;
    delete fileName;
    return 0;
}

string getExtension(const string &s);
string getExtension(const string &s)
{
    size_t pos = s.rfind('.');
    if (pos != string::npos) {
        return s.substr(pos + 1, s.length() - pos);
    }
    return "";
}

int main(int argc, char *argv[])
{
    bool ok = parseArgs(argDesc, &argc, argv);
    if (!ok || args.help) {
        printUsage(argv[0], "", argDesc);
        return ok ? 0 : -1;
    }
    if (strlen(args.format) == 0) {
        string extn = getExtension(args.outputFilename);
        if (extn == "hpgl") {
            strcpy(args.format, "hpgl");
        } else if (extn == "svg") {
            strcpy(args.format, "svg");
        } else {
            cerr << "Cannot guess output format from extension." << endl;
            printUsage(argv[0], "", argDesc);
            return -1;
        }
    }

    globalParams = std::make_unique<GlobalParams>();
    int rc = processPage();
    return rc;
}
