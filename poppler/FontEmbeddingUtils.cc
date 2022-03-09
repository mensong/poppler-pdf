//========================================================================
//
// FontEmbeddingUtils.cc
//
// Copyright (C) 2022 Georgiy Sgibnev <georgiy@sgibnev.com>. Work sponsored by lab50.net.
//
// This file is licensed under the GPLv2 or later
//
//========================================================================

#include <config.h>
#include <cstdio>
#include <sstream>
#include <vector>
#include <string>
#include <memory>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_FONT_FORMATS_H
#include FT_TRUETYPE_TABLES_H

#include "FontEmbeddingUtils.h"
#include "goo/gmem.h"
#include "Object.h"
#include "Array.h"
#include "PDFDoc.h"
#include "Dict.h"

namespace FontEmbeddingUtils {

// Font flags. See PDF 1.7 spec, 9.8.2.
typedef enum
{
    FIXED_PITCH_FONT = 1,
    SERIF_FONT = 1 << 1,
    SYMBOLIC_FONT = 1 << 2,
    SCRIPT_FONT = 1 << 3,
    NONSYMBOLIC_FONT = 1 << 5,
    ITALIC_FONT = 1 << 6,
    ALL_CAP_FONT = 1 << 16,
    SMALL_CAP_FONT = 1 << 17,
    FORCE_BOLD_FONT = 1 << 18
} PdfFontFlag;

// Supportive class for FreeTypeFont.
// Generates content of a Unicode map object.
class UnicodeMapEncoder
{
private:
    // Vector's value = Unicode value, vector's index = glyph index,
    // zero vector's value = there is no such glyph.
    std::vector<FT_ULong> glyphs;
    std::size_t rangesNum;
    std::size_t lonersNum;

public:
    // The class doesn't take ownership of face.
    explicit UnicodeMapEncoder(FT_Face face) : glyphs(face->num_glyphs, 0), rangesNum(0), lonersNum(0)
    {
        if (face->num_glyphs == 0) {
            return;
        }

        // Fill glyphs. Note: zero glyph index = there is no such glyph.
        FT_UInt glyphId;
        FT_ULong code = FT_Get_First_Char(face, &glyphId);
        while (glyphId > 0) {
            if (glyphId < (FT_ULong)face->num_glyphs) {
                glyphs[glyphId] = code;
            }
            code = FT_Get_Next_Char(face, code, &glyphId);
        }

        // Calculate rangesNum and lonersNum.
        std::size_t rangeLength = 0;
        const std::size_t glyphsNum = glyphs.size();
        for (std::size_t i = 0; i < glyphsNum; i += rangeLength) {
            rangeLength = getRangeLength(i);
            if (glyphs[i] > 0) {
                if (rangeLength > 1) {
                    rangesNum += 1;
                } else {
                    lonersNum += 1;
                }
            }
        }
    }

    UnicodeMapEncoder() = delete;
    UnicodeMapEncoder(const UnicodeMapEncoder &other) = delete;
    UnicodeMapEncoder(UnicodeMapEncoder &&other) = delete;
    UnicodeMapEncoder &operator=(const UnicodeMapEncoder &other) = delete;

    ~UnicodeMapEncoder() { }

    // Returns unicode map as string.
    std::string encode()
    {
        std::stringstream result;
        result << HEADER;
        encodeRanges(result);
        encodeLoners(result);
        result << FOOTER;
        return result.str();
    }

private:
    static const constexpr char *HEADER = "/CIDInit /ProcSet findresource begin\n"
                                          "12 dict begin\n"
                                          "begincmap\n"
                                          "/CIDSystemInfo <</Registry(Adobe)/Ordering(UCS)/Supplement 0>> def\n"
                                          "/CMapName /Adobe-Identity-UCS def\n"
                                          "/CMapType 2 def\n"
                                          "1 begincodespacerange\n"
                                          "<0000> <FFFF>\n"
                                          "endcodespacerange\n";
    static const constexpr char *FOOTER = "endcmap\n"
                                          "CMapName currentdict /CMap defineresource pop\n"
                                          "end\n"
                                          "end\n";
    static const constexpr char *RANGE_HEADER = " beginbfrange\n";
    static const constexpr char *RANGE_FOOTER = "endbfrange\n";
    static const constexpr char *LONER_HEADER = " beginbfchar\n";
    static const constexpr char *LONER_FOOTER = "endbfchar\n";

    // Range's Unicode values is a finite arithmetic progression with d = 1.
    std::size_t getRangeLength(std::size_t rangeStart)
    {
        std::size_t i;
        const std::size_t glyphsNum = glyphs.size();
        assert(rangeStart < glyphsNum);
        for (i = rangeStart + 1; i < glyphsNum; i++) {
            // Note: see Adobe Technical Note #5014 (Adobe CMap and CIDFont Files Specification).
            // "The count of codes within a given listed range that differ only in the last byte".
            if ((rangeStart & 0xFF00) != (i & 0xFF00)) {
                break;
            }
            if (glyphs[i] != glyphs[i - 1] + 1) {
                break;
            }
        }
        // i is an element after the range.
        return i - rangeStart;
    }

    // Dumps ranges to result.
    void encodeRanges(std::stringstream &result)
    {
        if (rangesNum == 0) {
            return;
        }
        if (rangesNum > 100) {
            result << 100 << RANGE_HEADER;
            rangesNum -= 100;
        } else {
            result << rangesNum << RANGE_HEADER;
        }

        int count = 0;
        char rangeBuffer[100] = { 0 };
        std::size_t rangeLength = 0;
        const std::size_t glyphsNum = glyphs.size();
        for (std::size_t i = 0; i < glyphsNum; i += rangeLength) {
            rangeLength = getRangeLength(i);
            if ((rangeLength > 1) && (glyphs[i] > 0)) {
                if (count == 100) {
                    result << RANGE_FOOTER;
                    if (rangesNum > 100) {
                        result << 100 << RANGE_HEADER;
                        rangesNum -= 100;
                    } else {
                        result << rangesNum << RANGE_HEADER;
                    }
                    count = 0;
                }
                snprintf(rangeBuffer, sizeof(rangeBuffer), "<%04lx> <%04lx> <%04lx>\n", i, i + rangeLength - 1, glyphs[i]);
                result << rangeBuffer;
                count += 1;
            }
        }
        result << RANGE_FOOTER;
    }

    // Dumps loners to result.
    void encodeLoners(std::stringstream &result)
    {
        if (lonersNum == 0)
            return;

        if (lonersNum > 100) {
            result << 100 << LONER_HEADER;
            lonersNum -= 100;
        } else {
            result << lonersNum << LONER_HEADER;
        }

        int count = 0;
        char rangeBuffer[100] = { 0 };
        std::size_t rangeLength = 0;
        const std::size_t glyphsNum = glyphs.size();
        for (std::size_t i = 0; i < glyphsNum; i += rangeLength) {
            rangeLength = getRangeLength(i);
            if ((rangeLength == 1) && (glyphs[i] > 0)) {
                if (count == 100) {
                    result << LONER_FOOTER;
                    if (lonersNum > 100) {
                        result << 100 << LONER_HEADER;
                        lonersNum -= 100;
                    } else {
                        result << lonersNum << LONER_HEADER;
                    }
                    count = 0;
                }
                snprintf(rangeBuffer, sizeof(rangeBuffer), "<%04lx> <%04lx>\n", i, glyphs[i]);
                result << rangeBuffer;
                count += 1;
            }
        }
        result << LONER_FOOTER;
    }
};

// Represents a font (a wrap around FT_Face pointer).
// Note: 1 unit of the user coordinate space = 1000 units of the glyph coordinate space.
// Notes from FreeType documentation:
// 1. You must not deallocate the memory before calling FT_Done_Face().
// 2. By default, FreeType enables a Unicode charmap.
class FreeTypeFont
{
public:
    // See FT_Get_Font_Format() documentation.
    static const constexpr char *TYPE1_FONT = "Type 1";
    static const constexpr char *TRUE_TYPE_FONT = "TrueType";
    static const constexpr char *CFF_FONT = "CFF";

    // Factory method. Returns an empty pointer in case of a failure.
    static std::unique_ptr<FreeTypeFont> open(const GooFile &fontFile)
    {
        // Load the font file.
        const Goffset fileSize = fontFile.size();
        if (fileSize < 0) {
            error(errIO, -1, "Font file size could not be calculated");
            return nullptr;
        }

        char *fileContent = new char[fileSize];
        const int n = fontFile.read(fileContent, fileSize, 0);
        if (n != fileSize) {
            delete[] fileContent;
            return nullptr;
        }

        // Init the FreeType stuff.
        FT_Library lib;
        FT_Error error = FT_Init_FreeType(&lib);
        if (error != 0) {
            fprintf(stderr, "Can't load the font. FT_Init_FreeType() failed.\n");
            delete[] fileContent;
            return nullptr;
        }
        FT_Face face;
        error = FT_New_Memory_Face(lib, (const FT_Byte *)fileContent, fileSize, 0, &face);
        if (error != 0) {
            fprintf(stderr, "Can't load the font. FT_New_Memory_Face() failed.\n");
            delete[] fileContent;
            FT_Done_FreeType(lib);
            return nullptr;
        }
        return std::unique_ptr<FreeTypeFont>(new FreeTypeFont(lib, face, fileContent, fileSize));
    }

    FreeTypeFont() = delete;
    FreeTypeFont(const FreeTypeFont &other) = delete;
    FreeTypeFont(FreeTypeFont &&other) = delete;
    FreeTypeFont &operator=(const FreeTypeFont &other) = delete;

    ~FreeTypeFont()
    {
        if (face != nullptr) {
            FT_Done_Face(face);
        }
        if (lib != nullptr) {
            FT_Done_FreeType(lib);
        }
        delete[] fileContent;
    }

    // Returns content of a unicode map object.
    std::string getUnicodeMap() const { return UnicodeMapEncoder(face).encode(); }

    std::string getName() const
    {
        if ((face->family_name != nullptr) && (face->style_name != nullptr)) {
            return std::string(face->family_name) + " " + face->style_name;
        } else if (face->family_name == nullptr) {
            return face->style_name;
        } else if (face->style_name == nullptr) {
            return face->family_name;
        }
        return "unknown";
    }
    std::string getPostscriptName() const { return FT_Get_Postscript_Name(face); }

    double getAscender() const { return face->ascender * 1000.0 / face->units_per_EM; }
    double getDescender() const { return face->descender * 1000.0 / face->units_per_EM; }
    double getXMin() const { return face->bbox.xMin * 1000.0 / face->units_per_EM; }
    double getYMin() const { return face->bbox.yMin * 1000.0 / face->units_per_EM; }
    double getXMax() const { return face->bbox.xMax * 1000.0 / face->units_per_EM; }
    double getYMax() const { return face->bbox.yMax * 1000.0 / face->units_per_EM; }

    FT_Long getGlyphsNum() const { return face->num_glyphs; }

    double getGlyphWidth(const FT_UInt glyphId) const
    {
        FT_Fixed advance = 0;
        FT_Get_Advance(face, glyphId, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_IGNORE_TRANSFORM, &advance);
        return advance * 1000.0 / face->units_per_EM;
    }

    // You own the returned pointer.
    char *getFileContentCopy() const
    {
        char *copy = (char *)gmalloc(fileSize);
        memcpy(copy, fileContent, fileSize);
        return copy;
    }
    Goffset getFileSize() const { return fileSize; }

    bool isType1() const { return (format == TYPE1_FONT); }
    bool isTrueType() const { return (format == TRUE_TYPE_FONT); }
    bool isCFF() const { return (format == CFF_FONT); }
    bool isSfntTableExists() const { return (FT_Get_Sfnt_Table(face, FT_SFNT_HEAD) != nullptr); }
    bool isUnicodeCharmapActive() const { return (face->charmap && (face->charmap->encoding == FT_ENCODING_UNICODE)); }

    bool isEmbeddingPermitted() const
    {
        const int flags = FT_Get_FSType_Flags(face);
        if (flags & FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING) {
            return false;
        }
        return true;
    }

private:
    // The class takes ownership of libA, faceA, fileContentA.
    FreeTypeFont(FT_Library libA, FT_Face faceA, char *fileContentA, Goffset fileSizeA) : lib(libA), face(faceA), fileContent(fileContentA), fileSize(fileSizeA), format(FT_Get_Font_Format(face)) { }

    // FreeType stuff.
    FT_Library lib;
    FT_Face face;

    // Font file content.
    char *fileContent;
    Goffset fileSize;

    std::string format;
};

// Supportive class for insertGlyphWidths().
// Use it to write glyph widths into an Array instance.
class GlyphWidthsWriter
{
public:
    // The class doesn't take ownership of widthsArray.
    GlyphWidthsWriter(XRef *xrefA, Array *widthsArray) : xref(xrefA), output(widthsArray), state(UNDEFINED), first(0), size(0), widths() { }

    ~GlyphWidthsWriter() { }

    // Processes new glyph.
    void write(int glyphWidth)
    {
        switch (state) {
        case SAME:
            if (widths.back() == glyphWidth) {
                size += 1;
            } else {
                write(output, first, first + size - 1, widths.back());
                resetBuffer(UNDEFINED, first + size, 1, glyphWidth);
            }
            break;
        case DIFFERENT:
            if (widths.back() == glyphWidth) {
                widths.pop_back();
                write(xref, output, first, widths);
                resetBuffer(SAME, first + size - 1, 2, glyphWidth);
            } else {
                size += 1;
                widths.push_back(glyphWidth);
            }
            break;
        case UNDEFINED:
            size += 1;
            if (size == 1) {
                widths.push_back(glyphWidth);
            } else if (size > 1) {
                if (widths.back() == glyphWidth) {
                    state = SAME;
                } else {
                    state = DIFFERENT;
                    widths.push_back(glyphWidth);
                }
            }
            break;
        }
    }

    // Call this function in the end to write the buffer to the output.
    void flush()
    {
        switch (state) {
        case SAME:
            write(output, first, first + size - 1, widths.back());
            break;
        case DIFFERENT:
            write(xref, output, first, widths);
            break;
        case UNDEFINED:
            if (size > 0) {
                write(xref, output, first, widths);
            }
            break;
        }
        state = UNDEFINED;
        size = 0;
        widths.clear();
    }

private:
    // UNDEFINED = 0 or 1 element in the buffer..
    // SAME = 2 or more same elements in the buffer.
    // DIFFERENT = 2 or more different elements in the buffer.
    enum BufferState
    {
        UNDEFINED,
        SAME,
        DIFFERENT
    };

    void resetBuffer(BufferState newState, int newFirst, int newSize, int glyphWidth)
    {
        state = newState;
        first = newFirst;
        size = newSize;
        widths.clear();
        widths.push_back(glyphWidth);
    }

    // Writes to output.
    static void write(Array *output, int first, int last, int glyphWidth)
    {
        output->add(Object(first));
        output->add(Object(last));
        output->add(Object(glyphWidth));
    }

    // Writes to output.
    static void write(XRef *xref, Array *output, int first, std::vector<int> &widths)
    {
        Array *childArray = new Array(xref);
        for (int &glyphWidth : widths) {
            childArray->add(Object(glyphWidth));
        }
        output->add(Object(first));
        output->add(Object(childArray));
    }

    XRef *xref;
    Array *output;
    BufferState state;
    int first;
    int size;
    std::vector<int> widths;
};

// Supportive function for insertDescendantFonts().
// Inserts new property "W" into the font object.
static void insertGlyphWidths(XRef *xref, Dict *fontDict, const FreeTypeFont &font)
{
    Goffset glyphsNum = font.getGlyphsNum();
    Array *widthsArray = new Array(xref);
    GlyphWidthsWriter writer = GlyphWidthsWriter(xref, widthsArray);
    for (Goffset i = 0; i < glyphsNum; i++) {
        writer.write(font.getGlyphWidth(i));
    }
    writer.flush();
    fontDict->add("W", Object(widthsArray));
}

// Supportive function for insertFontDescriptor().
// Inserts new property ("FontFile", "FontFile2" or "FontFile3") into the font descriptor object.
static void insertFontFile(XRef *xref, Dict *fontDescriptorDict, const FreeTypeFont &font)
{
    Dict *dict = new Dict(xref);
    if (font.isCFF()) {
        dict->add("Subtype", Object(objName, font.isSfntTableExists() ? "OpenType" : "Type1C"));
    }
    const Ref fontFileRef = xref->addStreamObject(dict, font.getFileContentCopy(), font.getFileSize());

    // Insert new property into the font descriptor object.
    if (font.isTrueType()) {
        fontDescriptorDict->add("FontFile2", Object(fontFileRef));
    } else if (font.isCFF()) {
        fontDescriptorDict->add("FontFile3", Object(fontFileRef));
    } else {
        fontDescriptorDict->add("FontFile", Object(fontFileRef));
    }
}

// Supportive function for insertFontDescriptor().
// Inserts new property "FontBBox" into the font descriptor object.
static void insertFontBBox(XRef *xref, Dict *fontDescriptorDict, const FreeTypeFont &font)
{
    Array *fontBBox = new Array(xref);
    fontBBox->add(Object(font.getXMin()));
    fontBBox->add(Object(font.getYMin()));
    fontBBox->add(Object(font.getXMax()));
    fontBBox->add(Object(font.getYMax()));
    fontDescriptorDict->add("FontBBox", Object(fontBBox));
}

// Supportive function for insertDescendantFonts().
// Inserts new property "FontDescriptor" into the font object.
static void insertFontDescriptor(XRef *xref, Dict *fontDict, const FreeTypeFont &font)
{
    Dict *fontDescriptorDict = new Dict(xref);
    fontDescriptorDict->add("Type", Object(objName, "FontDescriptor"));
    fontDescriptorDict->add("FontName", Object(objName, font.getName().c_str()));
    fontDescriptorDict->add("Flags", Object(int(SYMBOLIC_FONT)));
    fontDescriptorDict->add("Ascent", Object(font.getAscender()));
    fontDescriptorDict->add("Descent", Object(font.getDescender()));
    fontDescriptorDict->add("ItalicAngle", Object(0));
    fontDescriptorDict->add("StemV", Object(80));
    insertFontFile(xref, fontDescriptorDict, font);
    insertFontBBox(xref, fontDescriptorDict, font);
    const Ref fontDescriptorRef = xref->addIndirectObject(Object(fontDescriptorDict));
    fontDict->add("FontDescriptor", Object(fontDescriptorRef));
}

// Supportive function for insertDescendantFonts().
// Inserts new property "CIDSystemInfo" into the font object.
static void insertCIDSystemInfo(XRef *xref, Dict *fontDict)
{
    Dict *cidInfoDict = new Dict(xref);
    cidInfoDict->add("Ordering", Object(new GooString("Identity")));
    cidInfoDict->add("Registry", Object(new GooString("Adobe")));
    cidInfoDict->add("Supplement", Object(0));
    fontDict->add("CIDSystemInfo", Object(cidInfoDict));
}

// Supportive function for embedFont().
// Inserts new property "DescendantFonts" into the resource object.
static void insertDescendantFonts(XRef *xref, Dict *globalFontResourceDict, const FreeTypeFont &font)
{
    Dict *fontDict = new Dict(xref);
    fontDict->add("Type", Object(objName, "Font"));
    fontDict->add("Subtype", Object(objName, font.isTrueType() ? "CIDFontType2" : "CIDFontType0"));
    fontDict->add("BaseFont", Object(objName, font.getPostscriptName().c_str()));
    fontDict->add("DW", Object(0));
    fontDict->add("CIDToGIDMap", Object(objName, "Identity"));
    insertCIDSystemInfo(xref, fontDict);
    insertFontDescriptor(xref, fontDict, font);
    insertGlyphWidths(xref, fontDict, font);
    Ref fontRef = xref->addIndirectObject(Object(fontDict));

    Array *descendantFontsArray = new Array(xref);
    descendantFontsArray->add(Object(fontRef));
    globalFontResourceDict->add("DescendantFonts", Object(descendantFontsArray));
}

// Supportive function for embedFont().
// Inserts new property "ToUnicode" into the resource object.
static void insertUnicodeMap(XRef *xref, Dict *globalFontResourceDict, const FreeTypeFont &font)
{
    const std::string tmpBuffer = font.getUnicodeMap();
    char *buffer = (char *)gmalloc(tmpBuffer.length());
    memcpy(buffer, tmpBuffer.c_str(), tmpBuffer.length());
    Ref unicodeMapRef = xref->addStreamObject(new Dict(xref), buffer, tmpBuffer.length());
    globalFontResourceDict->add("ToUnicode", Object(unicodeMapRef));
}

Ref embed(XRef *xref, const GooFile &fontFile)
{
    auto font = FreeTypeFont::open(fontFile);
    if (!font) {
        return Ref::INVALID();
    }
    if (!font->isEmbeddingPermitted()) {
        fprintf(stderr, "Embedding isn't supported by the font's creator.\n");
        return Ref::INVALID();
    }

    if (font->isTrueType() || font->isCFF()) {
        if (!font->isUnicodeCharmapActive()) {
            fprintf(stderr, "A problem with Unicode charmap.\n");
            return Ref::INVALID();
        }

        Dict *globalFontResourceDict = new Dict(xref);
        globalFontResourceDict->add("Type", Object(objName, "Font"));
        globalFontResourceDict->add("Subtype", Object(objName, "Type0"));
        globalFontResourceDict->add("BaseFont", Object(objName, font->getName().c_str()));
        globalFontResourceDict->add("Encoding", Object(objName, "Identity-H"));
        insertDescendantFonts(xref, globalFontResourceDict, *font);
        insertUnicodeMap(xref, globalFontResourceDict, *font);
        return xref->addIndirectObject(Object(globalFontResourceDict));
    } else if (font->isType1()) {
        fprintf(stderr, "Type 1 format isn't supported.\n");
    } else {
        fprintf(stderr, "Font format isn't supported.\n");
    }
    return Ref::INVALID();
}

Ref embed(XRef *xref, const std::string &fontPath)
{
    std::unique_ptr<GooFile> fontFile(GooFile::open(fontPath));
    if (!fontFile) {
        error(errIO, -1, "Couldn't open {0:s}", fontPath.c_str());
        return Ref::INVALID();
    }
    return embed(xref, *fontFile);
}

}
