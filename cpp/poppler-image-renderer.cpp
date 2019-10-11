/*
 * Copyright (C) 2019, Zsombor Hollay-Horvath <hollay.horvath@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "GfxState.h"
#include "poppler-image-renderer.h"

using namespace poppler;

image_renderer::image_renderer(std::vector<image> &imagesA) {
    images = &imagesA;
}

image_renderer::~image_renderer() {

}

long image_renderer::getInlineImageLength(Stream *str, int width, int height,
                                          GfxImageColorMap *colorMap) {
    long len;

    if (colorMap) {
        ImageStream *imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
                                          colorMap->getBits());
        imgStr->reset();
        for (int y = 0; y < height; y++)
            imgStr->getLine();

        imgStr->close();
        delete imgStr;
    } else {
        str->reset();
        for (int y = 0; y < height; y++) {
            int size = (width + 7)/8;
            for (int x = 0; x < size; x++)
                str->getChar();
        }
    }

    EmbedStream *embedStr = (EmbedStream *) (str->getBaseStream());
    embedStr->rewind();
    len = 0;
    while (embedStr->getChar() != EOF)
        len++;

    embedStr->restore();

    return len;
}

void image_renderer::drawImage(GfxState *state, Object *ref, Stream *str,
             int width, int height,
             GfxImageColorMap *colorMap,
             bool interpolate, const int *maskColors, bool inlineImg) {
    ImageFormat format;
    image::format_enum outformat;
    char *imgdata = nullptr;
    bool monochrome = false;
    bool grayscale = false;
    EmbedStream *embedStr;
    ImageStream *imgStr = nullptr;
    unsigned char *rowp;
    unsigned char *p;
    GfxRGB rgb;
    //GfxCMYK cmyk;
    GfxGray gray;
    int invert_bits;

    if (inlineImg) {
        embedStr = (EmbedStream *) (str->getBaseStream());
        // Record the stream. This determines the size.
        getInlineImageLength(str, width, height, colorMap);
        // Reading the stream again will return EOF at end of recording.
        embedStr->rewind();
    }

    if (!colorMap || (colorMap->getNumPixelComps() == 1 && colorMap->getBits() == 1)) {
        format = imgMonochrome;
        monochrome = true;
    } else if (colorMap->getColorSpace()->getMode() == csDeviceGray ||
               colorMap->getColorSpace()->getMode() == csCalGray) {
        format = imgGray;
        grayscale = true;
    } else if (colorMap->getColorSpace()->getMode() == csDeviceCMYK ||
               (colorMap->getColorSpace()->getMode() == csICCBased && colorMap->getNumPixelComps() == 4)) {
        format = imgCMYK;
    } else if ((colorMap->getColorSpace()->getMode() == csDeviceRGB ||
               colorMap->getColorSpace()->getMode() == csCalRGB ||
               (colorMap->getColorSpace()->getMode() == csICCBased && colorMap->getNumPixelComps() == 3)) &&
               colorMap->getBits() > 8) {
        format = imgRGB48;
    } else {
        format = imgRGB;
    }

    int rowsize = 0;
    if (monochrome) {
        outformat = image::format_enum::format_mono;
        rowsize = (width + 7) >> 3;
    } else if (grayscale) {
        outformat = image::format_enum::format_gray8;
        rowsize = (width + 3) >> 2 << 2;
    } else {
        outformat = image::format_enum::format_rgb24;
        rowsize = (width * 3 + 3) >> 2 << 2;
    }

    image img = image(width, height, outformat);
    imgdata = img.data();

    if (format != imgMonochrome) {
        // initialize stream
        imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(),
                                 colorMap->getBits());
        imgStr->reset();
    } else {
        // initialize stream
        str->reset();
    }

    // PDF masks use 0 = draw current color, 1 = leave unchanged.
    // We invert this to provide the standard interpretation of alpha
    // (0 = transparent, 1 = opaque). If the colorMap already inverts
    // the mask we leave the data unchanged.
    invert_bits = 0xff;
    if (monochrome && colorMap) {
        unsigned char zero[gfxColorMaxComps];
        memset(zero, 0, sizeof(zero));
        colorMap->getGray(zero, &gray);
        if (colToByte(gray) == 0)
            invert_bits = 0x00;
    }

    // for each line...
    for (int y = 0; y < height; y++) {
        switch (format) {
        case imgRGB:
            p = imgStr->getLine();
            rowp = (unsigned char*)&imgdata[y*rowsize];
            for (int x = 0; x < width; ++x) {
                if (p) {
                    colorMap->getRGB(p, &rgb);
                    *rowp++ = colToByte(rgb.r);
                    *rowp++ = colToByte(rgb.g);
                    *rowp++ = colToByte(rgb.b);
                    p += colorMap->getNumPixelComps();
                } else {
                    *rowp++ = 0;
                    *rowp++ = 0;
                    *rowp++ = 0;
                }
            }
            break;

        case imgRGB48: {
            // FIXME: this is untested
            /*
            p = imgStr->getLine();
            Gushort *rowp16 = (Gushort*)row;
            for (int x = 0; x < width; ++x) {
                if (p) {
                    colorMap->getRGB(p, &rgb);
                    *rowp16++ = colToShort(rgb.r);
                    *rowp16++ = colToShort(rgb.g);
                    *rowp16++ = colToShort(rgb.b);
                    p += colorMap->getNumPixelComps();
                } else {
                    *rowp16++ = 0;
                    *rowp16++ = 0;
                    *rowp16++ = 0;
                }
            }
            if (writer)
                writer->writeRow(&row);
            */
            break;
        }

        case imgCMYK:
            // FIXME: this is untested
            /*
            p = imgStr->getLine();
            rowp = row;
            for (int x = 0; x < width; ++x) {
                if (p) {
                    colorMap->getCMYK(p, &cmyk);
                    *rowp++ = colToByte(cmyk.c);
                    *rowp++ = colToByte(cmyk.m);
                    *rowp++ = colToByte(cmyk.y);
                    *rowp++ = colToByte(cmyk.k);
                    p += colorMap->getNumPixelComps();
                } else {
                    *rowp++ = 0;
                    *rowp++ = 0;
                    *rowp++ = 0;
                    *rowp++ = 0;
                }
            }
            if (writer)
                writer->writeRow(&row);
            */
            break;

        case imgGray:
            p = imgStr->getLine();
            rowp = (unsigned char*)&imgdata[y*rowsize];
            for (int x = 0; x < width; ++x) {
                if (p) {
                    colorMap->getGray(p, &gray);
                    *rowp++ = colToByte(gray);
                    p += colorMap->getNumPixelComps();
                } else {
                    *rowp++ = 0;
                }
            }
            break;

        case imgMonochrome:
            for (int x = 0; x < rowsize; x++)
                imgdata[y*rowsize + x] = str->getChar() ^ invert_bits;
            break;
        }
    }

    if (format != imgMonochrome) {
        imgStr->close();
        delete imgStr;
    }
    str->close();

    images->push_back(img);
}
