/* poppler-private.cc: qt interface to poppler
 * Copyright (C) 2005, Net Integration Technologies, Inc.
 * Copyright (C) 2006, 2011, 2015, 2017, 2018 by Albert Astals Cid <aacid@kde.org>
 * Copyright (C) 2008, 2010, 2011, 2014 by Pino Toscano <pino@kde.org>
 * Copyright (C) 2013 by Thomas Freitag <Thomas.Freitag@alfa.de>
 * Copyright (C) 2013 Adrian Johnson <ajohnson@redneon.com>
 * Copyright (C) 2016 Jakub Alba <jakubalba@gmail.com>
 * Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, <info@kdab.com>. Work sponsored by the LiMux project of the city of Munich
 * Inspired on code by
 * Copyright (C) 2004 by Albert Astals Cid <tsdgeos@terra.es>
 * Copyright (C) 2004 by Enrico Ros <eros.kde@email.it>
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

#include "poppler-private.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QVariant>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <Link.h>
#include <Outline.h>
#include <PDFDocEncoding.h>
#if defined(HAVE_SPLASH)
#include <SplashOutputDev.h>
#include <splash/SplashBitmap.h>
#endif
#include <UnicodeMap.h>

namespace Poppler {

namespace Debug {

    static void qDebugDebugFunction(const QString &message, const QVariant & /*closure*/)
    {
        qDebug() << message;
    }

    PopplerDebugFunc debugFunction = qDebugDebugFunction;
    QVariant debugClosure;

}

    static UnicodeMap *utf8Map = nullptr;

    void setDebugErrorFunction(PopplerDebugFunc function, const QVariant &closure)
    {
        Debug::debugFunction = function ? function : Debug::qDebugDebugFunction;
        Debug::debugClosure = closure;
    }

    static void qt5ErrorFunction(void * /*data*/, ErrorCategory /*category*/, Goffset pos, const char *msg)
    {
        QString emsg;

        if (pos >= 0)
        {
            emsg = QStringLiteral("Error (%1): ").arg(pos);
        }
        else
        {
            emsg = QStringLiteral("Error: ");
        }
        emsg += QString::fromLatin1(msg);
        (*Debug::debugFunction)(emsg, Debug::debugClosure);
    }

    QString unicodeToQString(const Unicode* u, int len) {
        if (!utf8Map)
        {
                GooString enc("UTF-8");
                utf8Map = globalParams->getUnicodeMap(&enc);
                utf8Map->incRefCnt();
        }

        // ignore the last character if it is 0x0
        if ((len > 0) && (u[len - 1] == 0))
        {
            --len;
        }

        GooString convertedStr;
        for (int i = 0; i < len; ++i)
        {
            char buf[8];
            const int n = utf8Map->mapUnicode(u[i], buf, sizeof(buf));
            convertedStr.append(buf, n);
        }

        return QString::fromUtf8(convertedStr.getCString(), convertedStr.getLength());
    }

    QString UnicodeParsedString(const GooString *s1) {
        if ( !s1 || s1->getLength() == 0 )
            return QString();

        const char *cString;
        int stringLength;
        bool deleteCString;
        if ( ( s1->getChar(0) & 0xff ) == 0xfe && ( s1->getLength() > 1 && ( s1->getChar(1) & 0xff ) == 0xff ) )
        {
            cString = s1->getCString();
            stringLength = s1->getLength();
            deleteCString = false;
        }
        else
        {
            cString = pdfDocEncodingToUTF16(s1, &stringLength);
            deleteCString = true;
        }

        QString result;
        // i = 2 to skip the unicode marker
        for ( int i = 2; i < stringLength; i += 2 )
        {
            const Unicode u = ( ( cString[i] & 0xff ) << 8 ) | ( cString[i+1] & 0xff );
            result += QChar( u );
        }
        if (deleteCString)
            delete[] cString;
        return result;
    }

    GooString *QStringToUnicodeGooString(const QString &s) {
        int len = s.length() * 2 + 2;
        char *cstring = (char *)gmallocn(len, sizeof(char));
        cstring[0] = (char)0xfe;
        cstring[1] = (char)0xff;
        for (int i = 0; i < s.length(); ++i)
        {
            cstring[2+i*2] = s.at(i).row();
            cstring[3+i*2] = s.at(i).cell();
        }
        GooString *ret = new GooString(cstring, len);
        gfree(cstring);
        return ret;
    }

    GooString *QStringToGooString(const QString &s) {
        int len = s.length();
        char *cstring = (char *)gmallocn(s.length(), sizeof(char));
        for (int i = 0; i < len; ++i)
            cstring[i] = s.at(i).unicode();
        GooString *ret = new GooString(cstring, len);
        gfree(cstring);
        return ret;
    }

    GooString *QDateTimeToUnicodeGooString(const QDateTime &dt) {
        if (!dt.isValid()) {
            return nullptr;
        }

        return QStringToUnicodeGooString(dt.toUTC().toString("yyyyMMddhhmmss+00'00'"));
    }

    Annot::AdditionalActionsType toPopplerAdditionalActionType(Annotation::AdditionalActionType type) {
        switch ( type )
        {
            case Annotation::CursorEnteringAction:  return Annot::actionCursorEntering;
            case Annotation::CursorLeavingAction:   return Annot::actionCursorLeaving;
            case Annotation::MousePressedAction:    return Annot::actionMousePressed;
            case Annotation::MouseReleasedAction:   return Annot::actionMouseReleased;
            case Annotation::FocusInAction:         return Annot::actionFocusIn;
            case Annotation::FocusOutAction:        return Annot::actionFocusOut;
            case Annotation::PageOpeningAction:     return Annot::actionPageOpening;
            case Annotation::PageClosingAction:     return Annot::actionPageClosing;
            case Annotation::PageVisibleAction:     return Annot::actionPageVisible;
            case Annotation::PageInvisibleAction:   return Annot::actionPageInvisible;
        }

        return Annot::actionCursorEntering;
    }

    static void linkActionToTocItem( const ::LinkAction * a, DocumentData * doc, QDomElement * e )
    {
        if ( !a || !e )
            return;

        switch ( a->getKind() )
        {
            case actionGoTo:
            {
                // page number is contained/referenced in a LinkGoTo
                const LinkGoTo * g = static_cast< const LinkGoTo * >( a );
                const LinkDest * destination = g->getDest();
                if ( !destination && g->getNamedDest() )
                {
                    // no 'destination' but an internal 'named reference'. we could
                    // get the destination for the page now, but it's VERY time consuming,
                    // so better storing the reference and provide the viewport on demand
                    const GooString *s = g->getNamedDest();
                    QChar *charArray = new QChar[s->getLength()];
                    for (int i = 0; i < s->getLength(); ++i) charArray[i] = QChar(s->getCString()[i]);
                    QString aux(charArray, s->getLength());
                    e->setAttribute( QStringLiteral("DestinationName"), aux );
                    delete[] charArray;
                }
                else if ( destination && destination->isOk() )
                {
                    LinkDestinationData ldd(destination, nullptr, doc, false);
                    e->setAttribute( QStringLiteral("Destination"), LinkDestination(ldd).toString() );
                }
                break;
            }
            case actionGoToR:
            {
                // page number is contained/referenced in a LinkGoToR
                const LinkGoToR * g = static_cast< const LinkGoToR * >( a );
                const LinkDest * destination = g->getDest();
                if ( !destination && g->getNamedDest() )
                {
                    // no 'destination' but an internal 'named reference'. we could
                    // get the destination for the page now, but it's VERY time consuming,
                    // so better storing the reference and provide the viewport on demand
                    const GooString *s = g->getNamedDest();
                    QChar *charArray = new QChar[s->getLength()];
                    for (int i = 0; i < s->getLength(); ++i) charArray[i] = QChar(s->getCString()[i]);
                    QString aux(charArray, s->getLength());
                    e->setAttribute( QStringLiteral("DestinationName"), aux );
                    delete[] charArray;
                }
                else if ( destination && destination->isOk() )
                {
                    LinkDestinationData ldd(destination, nullptr, doc, g->getFileName() != nullptr);
                    e->setAttribute( QStringLiteral("Destination"), LinkDestination(ldd).toString() );
                }
                e->setAttribute( QStringLiteral("ExternalFileName"), g->getFileName()->getCString() );
                break;
            }
            case actionURI:
            {
                const LinkURI * u = static_cast< const LinkURI * >( a );
                e->setAttribute( QStringLiteral("DestinationURI"), u->getURI()->getCString() );
            }
            default: ;
        }
    }
    
    DocumentData::~DocumentData()
    {
        qDeleteAll(m_embeddedFiles);
        delete (OptContentModel *)m_optContentModel;
        delete doc;
    
        count --;
        if ( count == 0 )
        {
            utf8Map = nullptr;
            delete globalParams;
        }
      }
    
    void DocumentData::init()
    {
        m_backend = Document::SplashBackend;
        paperColor = Qt::white;
        m_hints = 0;
        m_optContentModel = nullptr;
      
        if ( count == 0 )
        {
            utf8Map = nullptr;
            globalParams = new GlobalParams();
            setErrorCallback(qt5ErrorFunction, nullptr);
        }
        count ++;
    }


    void DocumentData::addTocChildren( QDomDocument * docSyn, QDomNode * parent, const GooList * items )
    {
        int numItems = items->getLength();
        for ( int i = 0; i < numItems; ++i )
        {
            // iterate over every object in 'items'
            OutlineItem * outlineItem = (OutlineItem *)items->get( i );

            // 1. create element using outlineItem's title as tagName
            QString name;
            const Unicode * uniChar = outlineItem->getTitle();
            int titleLength = outlineItem->getTitleLength();
            name = unicodeToQString(uniChar, titleLength);
            if ( name.isEmpty() )
                continue;

            QDomElement item = docSyn->createElement( name );
            parent->appendChild( item );

            // 2. find the page the link refers to
            const ::LinkAction * a = outlineItem->getAction();
            linkActionToTocItem( a, this, &item );

            item.setAttribute( QStringLiteral("Open"), QVariant( (bool)outlineItem->isOpen() ).toString() );

            // 3. recursively descend over children
            outlineItem->open();
            const GooList * children = outlineItem->getKids();
            if ( children )
                addTocChildren( docSyn, &item, children );
        }
    }

    ArthurRenderSetup::ArthurRenderSetup(QPainter* painter)
        : p_painter(painter), m_savePainter(false) {
    }

    QPainter* ArthurRenderSetup::painter() {
        return p_painter;
    }

    QImage* ArthurRenderSetup::destImage() {
        return nullptr;
    }

    ArthurRenderSetup::~ArthurRenderSetup() {
    }

    void ArthurRenderSetup::setupPainter(int docHints, int pageFlags, int x, int y) {
        m_savePainter = !(pageFlags & Poppler::Page:: DontSaveAndRestore);
        if (m_savePainter)
            p_painter->save();
        if (docHints & Poppler::Document::Antialiasing)
            p_painter->setRenderHint(QPainter::Antialiasing);
        if (docHints & Poppler::Document::TextAntialiasing)
            p_painter->setRenderHint(QPainter::TextAntialiasing);
        p_painter->translate(x == -1 ? 0 : -x, y == -1 ? 0 : -y);
    }

    void ArthurRenderSetup::restore() {
        if (m_savePainter)
            p_painter->restore();
    }

    ClientArthurRenderSetup::ClientArthurRenderSetup(int x, int y, QPainter* painter, int docHints, int pageFlags)
        : ArthurRenderSetup(painter) {
        setupPainter(docHints, pageFlags, x, y);
    }

    ClientArthurRenderSetup::~ClientArthurRenderSetup() {
        restore();
    }

    ImageArthurRenderSetup::ImageArthurRenderSetup(int x, int y, int w, int h, double xres, double yres, QSize pageSize, int pageFlags, QColor& docPaperColor, int docHints)
        : ArthurRenderSetup(&m_painter)
        , m_image(scale(pageSize, xres, yres, w, h), QImage::Format_ARGB32)
        , m_painter(&m_image) {
        m_image.fill(docPaperColor);
        setupPainter(docHints, pageFlags, x, y);
    }

    QImage* ImageArthurRenderSetup::destImage() {
        return &m_image;
    }

    ImageArthurRenderSetup::~ImageArthurRenderSetup() {
        restore();
        m_painter.end();
    }

    QSize ImageArthurRenderSetup::scale(QSize pageSize, double xres, double yres, int w, int h) const {
        return QSize(w == -1 ? qRound( pageSize.width() * xres / 72.0 ) : w,
                     h == -1 ? qRound( pageSize.height() * yres / 72.0) : h );
    }

    SplashRenderSetup::SplashRenderSetup(const int docHints, QColor docPaperColor)
        : bitmapRowPad(4)
        , reverseVideo(false)
        , ignorePaperColorA(docHints & Poppler::Document::IgnorePaperColor)
        , bitmapTopDown(true)
        , overprintPreview(false) {
#ifdef SPLASH_CMYK
        overprintPreview = docHints & Document::OverprintPreview ? true : false;
        if (overprintPreview)
        {
            Guchar c, m, y, k;

            c = 255 - docPaperColor.blue();
            m = 255 - docPaperColor.red();
            y = 255 - docPaperColor.green();
            k = c;
            if (m < k) {
                k = m;
            }
            if (y < k) {
                k = y;
            }
            bgColor[0] = c - k;
            bgColor[1] = m - k;
            bgColor[2] = y - k;
            bgColor[3] = k;
            for (int i = 4; i < SPOT_NCOMPS + 4; i++) {
                bgColor[i] = 0;
            }
        }
        else
#endif
        {
            bgColor[0] = docPaperColor.blue();
            bgColor[1] = docPaperColor.green();
            bgColor[2] = docPaperColor.red();
        }

        colorMode = splashModeXBGR8;
#ifdef SPLASH_CMYK
        if (overprintPreview) colorMode = splashModeDeviceN8;
#endif

        thinLineMode = splashThinLineDefault;
        if (docHints & Poppler::Document::ThinLineShape)
            thinLineMode = splashThinLineShape;
        if (docHints & Poppler::Document::ThinLineSolid)
            thinLineMode = splashThinLineSolid;

        paperColor = ignorePaperColorA ? nullptr : bgColor;

        textAntialiasing = docHints & Poppler::Document::TextAntialiasing ? true : false;
        vectorAntialias = docHints & Poppler::Document::Antialiasing ? true : false;
        freeTypeHintingEnable = docHints & Poppler::Document::TextHinting ? true : false;
        freeTypeHintingEnableSlightHintingA = docHints & Poppler::Document::TextSlightHinting ? true : false;
    }

    void OutputDevCallbackHelper::setCallbacks(Page::RenderToImagePartialUpdateFunc callback, Page::ShouldRenderToImagePartialQueryFunc shouldDoCallback, Page::ShouldAbortQueryFunc shouldAbortCallback, const QVariant &payloadA)
    {
        partialUpdateCallback = callback;
        shouldDoPartialUpdateCallback = shouldDoCallback;
        shouldAbortRenderCallback = shouldAbortCallback;
        payload = payloadA;
    }

    Qt5SplashOutputDev::Qt5SplashOutputDev(const SplashRenderSetup& settings)
        : SplashOutputDev(settings.colorMode, settings.bitmapRowPad, settings.reverseVideo
        , settings.paperColor, settings.bitmapTopDown
        , settings.thinLineMode, settings.overprintPreview)
        , ignorePaperColor(settings.ignorePaperColorA) {
        setFontAntialias(settings.textAntialiasing);
        setVectorAntialias(settings.vectorAntialias);
        setFreeTypeHinting(settings.freeTypeHintingEnable, settings.freeTypeHintingEnableSlightHintingA);
    }

    void Qt5SplashOutputDev::dump()
    {
        if (partialUpdateCallback && shouldDoPartialUpdateCallback && shouldDoPartialUpdateCallback(payload)) {
            partialUpdateCallback(getXBGRImage( false /* takeImageData */), payload);
        }
    }

    QImage Qt5SplashOutputDev::getXBGRImage(bool takeImageData)
    {
        SplashBitmap *b = getBitmap();

        const int bw = b->getWidth();
        const int bh = b->getHeight();
        const int brs = b->getRowSize();

        // If we use DeviceN8, convert to XBGR8.
        // If requested, also transfer Splash's internal alpha channel.
        const SplashBitmap::ConversionMode mode = ignorePaperColor
                ? SplashBitmap::conversionAlphaPremultiplied
                : SplashBitmap::conversionOpaque;

        const QImage::Format format = ignorePaperColor
                ? QImage::Format_ARGB32_Premultiplied
                : QImage::Format_RGB32;

        if (b->convertToXBGR(mode)) {
            SplashColorPtr data = takeImageData ? b->takeData() : b->getDataPtr();

            if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
                // Convert byte order from RGBX to XBGR.
                for (int i = 0; i < bh; ++i) {
                    for (int j = 0; j < bw; ++j) {
                        SplashColorPtr pixel = &data[i * brs + j];

                        qSwap(pixel[0], pixel[3]);
                        qSwap(pixel[1], pixel[2]);
                    }
                }
            }

            if (takeImageData) {
                // Construct a Qt image holding (and also owning) the raw bitmap data.
                return QImage(data, bw, bh, brs, format, gfree, data);
            } else {
                return QImage(data, bw, bh, brs, format).copy();
            }
        }

        return QImage();
    }

    void Qt5SplashOutputDev::startDoc(PDFDoc *docA)
    {
        SplashOutputDev::startDoc(docA);
    }

    OutputDev* Qt5SplashOutputDev::outputDev()
    {
        return this;
    }

    OutputDevCallbackHelper* Qt5SplashOutputDev::callbackHelper()
    {
        return this;
    }

    QImageDumpingArthurOutputDev::QImageDumpingArthurOutputDev(ArthurRenderSetup& renderSetup)
        : ArthurOutputDev(renderSetup.painter()), m_renderSetup(renderSetup) {
    }

    void QImageDumpingArthurOutputDev::dump()
    {
        if (partialUpdateCallback && shouldDoPartialUpdateCallback && shouldDoPartialUpdateCallback(payload)) {
            partialUpdateCallback(*m_renderSetup.destImage(), payload);
        }
    }

    QImage QImageDumpingArthurOutputDev::getImage() const {
        QImage* image = m_renderSetup.destImage();
        if (image) {
            return *image;
        }
        return QImage();
    }

    void QImageDumpingArthurOutputDev::startDoc(PDFDoc *docA)
    {
        ArthurOutputDev::startDoc(docA);
    }

    OutputDev* QImageDumpingArthurOutputDev::outputDev()
    {
        return this;
    }

    OutputDevCallbackHelper* QImageDumpingArthurOutputDev::callbackHelper()
    {
        return this;
    }
}
