//========================================================================
//
// HersheyFont.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "HersheyFont.h"
#include "Plot.h"

using namespace std;

void strip_nl(string *str);
void strip_nl(string *str)
{
    if (!str->empty() && (*str)[str->size() - 1] == '\n') {
        str->erase(str->size() - 1);
    }
}

int valuePastR(char rBasedValue);
int valuePastR(char rBasedValue)
{
    return rBasedValue - 'R';
}

HersheyFont *HersheyFont::load(const string &font_file_name, bool oblique)
{
    HersheyFont *font = new HersheyFont(font_file_name, oblique);
    if (font->valid()) {
        return font;
    }
    delete font;
    return nullptr;
}

HersheyFont::HersheyFont(const string &font_file_name, bool oblique)
{
    ifstream infile;
    infile.open(font_file_name);
    if (!infile.is_open()) {
        return; // Error reported as !valid()
    }
    string line;
    int line_num_with_glyph = -1;
    while (getline(infile, line)) {
        if (line == "") {
            break; // EOF
        }
        if (line == "\x1a") {
            break; // a weird EOF
        }
        line_num_with_glyph += 1;
        strip_nl(&line);
        int char_id = stoi(line.substr(0, 5));
        if (char_id == 12345) {
            char_id = line_num_with_glyph;
        }
        unsigned n_vertices = stoi(line.substr(5, 3)) - 1;
        // TODO we center the characters right now, but we might get better placements
        // by using the positional info in the hershey font.
        // int left_hand = valuePastR(line[8]);
        // int right_hand = valuePastR(line[9]);
        while (true) {
            string v_list = line.substr(10);
            if (v_list.size() == n_vertices * 2) {
                break;
            }
            if (!(v_list.size() < n_vertices * 2)) {
                return;
            }
            string extn;
            getline(infile, extn);
            assert(extn == "");
            strip_nl(&extn);
            line += extn;
        }

        SplashPath *path = new SplashPath();
        bool nextIsMoveTo = true;
        path->reserve(n_vertices);
        int min_x = 500, max_x = -500;
        for (unsigned vertex = 0; vertex < n_vertices; vertex++) {
            string sym = line.substr(10 + vertex * 2, 2);
            if (sym == " R") {
                // pen up
                nextIsMoveTo = true;
            } else {
                int x = valuePastR(sym[0]);
                int y = -valuePastR(sym[1]);
                min_x = min(min_x, x);
                max_x = max(max_x, x);
                if (nextIsMoveTo) {
                    path->moveTo(x, y);
                    nextIsMoveTo = false;
                } else {
                    path->lineTo(x, y);
                }
            }
        }
        if (oblique) {
            // Tilt by 20%; that fits nicely with Arial Italic and friends.
            Matrix oblique_matrix = { 1, 0, 0.2, 1, 0, 0 };
            SplashPath *pNew = path->transform(PlotMatrix(oblique_matrix).asSplashMatrix());
            delete path;
            path = pNew;
        }
        if (path->getLength() > 0) {
            glyphs[(char)char_id + ' '] = Glyph { path,
                                                  /*width*/ (double)max_x - min_x,
                                                  /*xShift*/ (double)-min_x };
        } else {
            delete path;
        }
    }

    if (glyphs.count('M') == 0) {
        yShift = 0.0;
    } else {
        SplashPath *glyphM = glyphs.at('M').path;
        double minY = 0.0;
        for (int i = 0; i < glyphM->getLength(); i++) {
            double x, y;
            unsigned char f;
            glyphM->getPoint(i, &x, &y, &f);
            minY = min(minY, y);
        }
        yShift = -minY;
    }

    // Apply corrective transforms
    for (auto &it : glyphs) {
        Glyph &glyph = it.second;
        SplashPath *pOld = glyph.path;
        Matrix translate = { 1, 0, 0, 1, glyph.xShift - glyph.width / 2, yShift };
        SplashPath *pNew = pOld->transform(PlotMatrix(translate).asSplashMatrix());
        glyph = Glyph { pNew, glyph.width, 0.0 };
        delete pOld;
    }

    _valid = true;
}

HersheyFont::~HersheyFont()
{
    for (auto it = glyphs.begin(); it != glyphs.end();) {
        delete it->second.path;
        it = glyphs.erase(it);
    }
}

void HersheyFont::makeLigature(Unicode c, Unicode *translation) { }

template<class T>
class Freer
{
public:
    Freer(T *_ptr) : ptr(_ptr) { }
    Freer(const Freer &) = delete;
    ~Freer() { gfree(ptr); }

private:
    T *ptr;
};

void HersheyFont::makeChar(Unicode uc)
{
    Unicode *translation;
    int out_len;

    unicodeToAscii7(&uc, 1, &translation, &out_len, nullptr, nullptr);
    Freer<Unicode> deleter(translation);

    if (out_len == 0) {
        // No translation
        return;
    }
    if (translation[0] > 255) {
        // Useless translation
        return;
    }

    // Copy possibly-multiple glyphs (eg ligature) into a fresh path
    Glyph out_glyph;
    out_glyph.path = new SplashPath();
    out_glyph.width = 0;
    for (int i = 0; i < out_len; i++) {
        Unicode txln = translation[i];
        if (glyphs.count(txln) == 0) {
            cout << "translation failure from " << uc << " to " << txln << endl;
            return;
        }
        Glyph *og = &glyphs[txln];
        Matrix matrix = { 1, 0, 0, 1, out_glyph.width, 0 };
        SplashPath *transformed = og->path->transform(PlotMatrix(matrix).asSplashMatrix());
        out_glyph.path->append(transformed);
        delete transformed;
        out_glyph.width += og->width;
    }
    // Hershey glyphs are stored centered so they can be displayed by aligning
    // with the center of the original outline glyph.
    PlotBBox bbox = PlotBBox::fromSplashPath(out_glyph.path);
    double half_width = (bbox.right - bbox.left) / 2;
    Matrix matrix = { 1, 0, 0, 1, -bbox.left - half_width, 0 };
    SplashPath *pre_path = out_glyph.path;
    out_glyph.path = pre_path->transform(PlotMatrix(matrix).asSplashMatrix());
    delete pre_path;
    bbox = PlotBBox::fromSplashPath(out_glyph.path);
    glyphs[uc] = out_glyph;
}

SplashPath *HersheyFont::getChar(Unicode uc)
{
    if (glyphs.count(uc) == 0) {
        makeChar(uc);
    }
    if (glyphs.count(uc) == 0) {
        return nullptr;
    }
    return glyphs[uc].path;
}

bool HersheyFont::haveNativeChar(Unicode uc)
{
    return glyphs.count(uc) != 0;
}

vector<Unicode> HersheyFont::glyphSet()
{
    vector<Unicode> glyphSet;
    for (auto &it : glyphs) {
        glyphSet.push_back(it.first);
    }
    return glyphSet;
}
