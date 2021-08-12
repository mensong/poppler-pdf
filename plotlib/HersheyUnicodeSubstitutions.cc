//========================================================================
//
// HersheyUnicodeSubstitutions.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#include <algorithm>
#include <iostream>
#include <sstream>
#include "HersheyUnicodeSubstitutions.h"

using namespace std;

HersheyFontDescriptor HersheyUnicodeSubstitutions::greek_font = { { "greek", false }, nullptr };

UnicodeHersheyMapping HersheyUnicodeSubstitutions::greek_mapping[] = {
    // python -c 'for i in range(0x397,0x3a9+1): print("  {0x%x,0x00}," % i);'
    { 0x0391, 0x41 },
    { 0x0392, 0x42 },
    { 0x0393, 0x47 },
    { 0x0394, 0x44 },
    { 0x0395, 0x45 },
    { 0x0396, 0x5a },
    { 0x0397, 0x48 },
    { 0x0398, 0x51 },
    { 0x0399, 0x49 },
    { 0x039a, 0x4b },
    { 0x039b, 0x4c },
    { 0x039c, 0x4d },
    { 0x039d, 0x4e },
    { 0x039e, 0x58 },
    { 0x039f, 0x4f },
    { 0x03a0, 0x50 },
    { 0x03a1, 0x52 },
    { 0x03a3, 0x53 },
    { 0x03a4, 0x54 },
    { 0x03a5, 0x55 },
    { 0x03a6, 0x46 },
    { 0x03a7, 0x43 },
    { 0x03a8, 0x59 },
    { 0x03a9, 0x57 },
    // Lowercase is just uppercase + 0x20
    { 0x03b1, 0x61 },
    { 0x03b2, 0x62 },
    { 0x03b3, 0x67 },
    { 0x03b4, 0x64 },
    { 0x03b5, 0x65 },
    { 0x03b6, 0x7a },
    { 0x03b7, 0x68 },
    { 0x03b8, 0x71 },
    { 0x03b9, 0x69 },
    { 0x03ba, 0x6b },
    { 0x03bb, 0x6c },
    { 0x03bc, 0x6d },
    { 0x03bd, 0x6e },
    { 0x03be, 0x78 },
    { 0x03bf, 0x6f },
    { 0x03c0, 0x70 },
    { 0x03c1, 0x72 },
    { 0x03c3, 0x73 },
    { 0x03c4, 0x74 },
    { 0x03c5, 0x75 },
    { 0x03c6, 0x66 },
    { 0x03c7, 0x63 },
    { 0x03c8, 0x79 },
    { 0x03c9, 0x77 },
    { 0x0, 0x0 }, // terminator sentinel
};

HersheyFontDescriptor HersheyUnicodeSubstitutions::futuram_font = { { "futuram", false }, nullptr };

// We're only here for the big angle brackets.
UnicodeHersheyMapping HersheyUnicodeSubstitutions::futuram_mapping[] = {
    { 0x2329, 0x7b }, { 0x3008, 0x7b }, { 0x232a, 0x7d }, { 0x3009, 0x7d }, { 0x0, 0x0 }, // terminator sentinel
};

HersheyFontDescriptor HersheyUnicodeSubstitutions::symbolic_font = { { "symbolic", false }, nullptr };

UnicodeHersheyMapping HersheyUnicodeSubstitutions::symbolic_mapping[] = {
    { 0x2022, 0x41 }, // bullet
    { 0x2227, 0x45 }, // logical and
    { 0x26db, 0x47 }, // down triangle
    { 0x25bc, 0x47 }, // down triangle
    { 0x25cb, 0x48 }, // circle
    { 0x25a1, 0x49 }, // square
    { 0x25b3, 0x4a }, // up triangle
    { 0x25ca, 0x4b }, // narrow diamond
    { 0x2606, 0x4c }, // star
    //  {0x2a09, 0x4e}, // times
    { 0x25cf, 0x50 }, // black circle
    { 0x25fc, 0x51 }, // black square
    { 0x23f9, 0x51 }, // black square
    { 0x25a0, 0x51 }, // black square
    { 0x25b2, 0x52 }, // black up-pointing triangle
    { 0x25c0, 0x53 }, // black left-pointing triangle
    { 0x25bc, 0x54 }, // black down-pointing triangle
    { 0x25b6, 0x55 }, // black right-pointing triangle
    { 0x2693, 0x58 }, // anchor TODO inkscape ate it
    { 0x2692, 0x5a }, // hammer and pick
    { 0x2719, 0x5e }, // outlined greek cross
    { 0x2721, 0x60 }, // star of david
    { 0x1f514, 0x61 }, // bell TODO inkscape ate it
    { 0x2663, 0x68 }, // club
    { 0x00a7, 0x69 }, // club
    { 0x2020, 0x6a }, // dagger
    { 0x2021, 0x6b }, // double dagger
    //  {0x2234, 0x6c}, // therefore
    { 0x2664, 0x6d }, // spades
    { 0x2661, 0x6e }, // hearts
    { 0x2662, 0x6f }, // diamonds
    { 0x2667, 0x70 }, // clubs
    { 0x2660, 0x6d }, // black spades
    { 0x2665, 0x6e }, // black hearts
    { 0x2666, 0x6f }, // black diamonds
    { 0x2663, 0x70 }, // black clubs
    { 0xff5b, 0x71 }, // huge left curly
    { 0xff5d, 0x72 }, // huge right curly
    { 0x2605, 0x7e }, // black star
    { 0x0, 0x0 }, // terminator sentinel
};

HersheyFontDescriptor HersheyUnicodeSubstitutions::mathlow_font = { { "mathlow", false }, nullptr };

UnicodeHersheyMapping HersheyUnicodeSubstitutions::mathlow_mapping[] = {
    { 0x571f, 0x21 }, // TODO gets lost; inkscape translation?
    { 0x5e72, 0x22 }, // TODO gets lost; inkscape translation?
    { 0x2a09, 0x23 }, // times
    { 0x22c5, 0x24 }, // dot operator
    { 0x2266, 0x26 }, // less than over equal to
    { 0x2267, 0x27 }, // greater than over equal to
    { 0x220f, 0x3a }, // n-ary product
    { 0x2211, 0x3b }, // n-ary sum
    { 0x2260, 0x3f }, // not equal to
    { 0x2261, 0x40 }, // identical to
    { 0x221d, 0x5e }, // proportional to
    { 0x221e, 0x5f }, // infinity
    { 0x00b0, 0x60 }, // degree
    { 0x23b7, 0x62 }, // radical (0x63 is another one)
    { 0x2282, 0x64 }, // subset
    { 0x222a, 0x65 }, // subset
    { 0x2283, 0x66 }, // subset
    { 0x2229, 0x67 }, // intersection
    { 0x2208, 0x68 }, // member
    { 0x2192, 0x69 }, // right arrow
    { 0x2191, 0x6a }, // upwards arrow
    { 0x2190, 0x6b }, // left arrow
    { 0x2193, 0x6c }, // down arrow
    { 0x222b, 0x70 }, // integral
    //  {0x222b, 0x71}, // huge integral
    { 0x2203, 0x76 }, // exists
    { 0x00f7, 0x78 }, // division
    { 0x2225, 0x79 }, // parallel
    { 0x27c2, 0x7a }, // perpendicular
    { 0x2220, 0x7c }, // angle
    { 0x2234, 0x7e }, // therefore
    { 0x0, 0x0 }, // terminator sentinel
};

void HersheyUnicodeSubstitutions::warnMissingFont(const string &fontName)
{
    cerr << "Can't load Hershey font " << fontName << endl;
    if (!emittedMissingHersheyFontWarning) {
        cerr << "  Falling back to stroking outline fonts." << endl;
        cerr << "  Looking in " << hershey_directory->getHersheyFontPath() << endl;
        cerr << "  Override default path with environment variable " << HersheyDirectory::HERSHEY_FONT_PATH_ENV << endl;
        emittedMissingHersheyFontWarning = true;
    }
}

void HersheyUnicodeSubstitutions::LoadMapping(HersheyFontDescriptor *descriptor, const UnicodeHersheyMapping *mapping)
{
    if (descriptor->loaded_font == nullptr) {
        HersheyFont *loaded_font = hershey_directory->openHersheyFont(descriptor->req);
        if (!loaded_font) {
            warnMissingFont(descriptor->req.hershey_font_name);
            delete loaded_font;
            return;
        }
        descriptor->loaded_font = loaded_font;
    }

    for (int i = 0; mapping[i].u != 0; i++) {
        passthrough_table[mapping[i].u] = GlyphPtr(descriptor, mapping[i].h);
    }
}

HersheyUnicodeSubstitutions::HersheyUnicodeSubstitutions(HersheyDirectory *_hershey_directory) : hershey_directory(_hershey_directory), emittedMissingHersheyFontWarning(false)
{
    LoadMapping(&greek_font, greek_mapping);
    LoadMapping(&futuram_font, futuram_mapping);
    LoadMapping(&symbolic_font, symbolic_mapping);
    LoadMapping(&mathlow_font, mathlow_mapping);
}

SplashPath *HersheyUnicodeSubstitutions::getChar(HersheyFont *hershey_font, Unicode c)
{
    if (hershey_font->haveNativeChar(c)) {
        // requested font has the desired unicode char natively.
        return hershey_font->getChar(c);
    } else if (passthrough_table.count(c) != 0) {
        // We do have a cross-font translation, so prefer that.
        GlyphPtr m = passthrough_table[c];
        return m.f->loaded_font->getChar(m.h);
    } else {
        // We don't have a clever cross-font translation, so try
        // ASCII7 fallback in the requested font.
        return hershey_font->getChar(c);
    }
}

wstring HersheyUnicodeSubstitutions::getKnownSubstitutionsForDebug()
{
    vector<Unicode> syms;
    for (auto &it : passthrough_table) {
        Unicode uc = it.first;
        syms.push_back(uc);
    }
    sort(syms.begin(), syms.end());

    wstringstream result;
    for (auto uc : syms) {
        result << (wchar_t)uc;
    }
    return result.str();
}
