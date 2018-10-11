#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>

#include <QList>
#include <QLinkedList>
#include <QMetaType>
#include <QRgb>
#include <QtTest/QtTest>

#include <poppler-qt5.h>

#include "goo/GooString.h"
#include "goo/gstrtod.h"

struct ColorWithRatio{
  QRgb color;
  double ratio;
};

Q_DECLARE_METATYPE(QList<ColorWithRatio>)
Q_DECLARE_METATYPE(std::shared_ptr<Poppler::Document>)
Q_DECLARE_METATYPE(std::shared_ptr<Poppler::Page>)
Q_DECLARE_METATYPE(std::shared_ptr<Poppler::Annotation>)
Q_DECLARE_METATYPE(Poppler::Document::RenderBackend)

class TestAnnotations : public QObject
{
  Q_OBJECT
private slots:
  void checkQColorPrecision();
  void checkFontSizeAndColor();
  void checkRenderToImage_data();
  void checkRenderToImage();
};

/* Is .5f sufficient for 16 bit color channel roundtrip trough save and load on all architectures? */
void TestAnnotations::checkQColorPrecision() {
  bool precisionOk = true;
  for (int i = std::numeric_limits<uint16_t>::min(); i <= std::numeric_limits<uint16_t>::max(); i++) {
    double normalized = static_cast<uint16_t>(i) / static_cast<double>(std::numeric_limits<uint16_t>::max());
    GooString* serialized = GooString::format("{0:.5f}", normalized);
    double deserialized = gatof( serialized->getCString() );
    uint16_t denormalized = std::round(deserialized * std::numeric_limits<uint16_t>::max());
    if (static_cast<uint16_t>(i) != denormalized) {
      precisionOk = false;
      break;
    }
  }
  QVERIFY(precisionOk);
}

void TestAnnotations::checkFontSizeAndColor()
{
  const QString contents{"foobar"};
  const std::vector<QColor> testColors{QColor::fromRgb(0xAB, 0xCD, 0xEF),
                                       QColor::fromCmyk(0xAB, 0xBC, 0xCD, 0xDE)};
  const QFont testFont("Helvetica", 20);

  QTemporaryFile tempFile;
  QVERIFY(tempFile.open());
  tempFile.close();

  {
    std::unique_ptr<Poppler::Document> doc{
      Poppler::Document::load(TESTDATADIR "/unittestcases/UseNone.pdf")
    };
    QVERIFY(doc.get());

    std::unique_ptr<Poppler::Page> page{
      doc->page(0)
    };
    QVERIFY(page.get());

    for (const auto& color : testColors) {
      auto annot = std::make_unique<Poppler::TextAnnotation>(Poppler::TextAnnotation::InPlace);
      annot->setBoundary(QRectF(0.0, 0.0, 1.0, 1.0));
      annot->setContents(contents);
      annot->setTextFont(testFont);
      annot->setTextColor(color);
      page->addAnnotation(annot.get());
    }

    std::unique_ptr<Poppler::PDFConverter> conv(doc->pdfConverter());
    QVERIFY(conv.get());
    conv->setOutputFileName(tempFile.fileName());
    conv->setPDFOptions(Poppler::PDFConverter::WithChanges);
    QVERIFY(conv->convert());
  }

  {
    std::unique_ptr<Poppler::Document> doc{
      Poppler::Document::load(tempFile.fileName())
    };
    QVERIFY(doc.get());

    std::unique_ptr<Poppler::Page> page{
      doc->page(0)
    };
    QVERIFY(page.get());

    auto annots = page->annotations();
    QCOMPARE(annots.size(), static_cast<int>(testColors.size()));

    auto &&annot = annots.constBegin();
    for (const auto& color : testColors) {
      QCOMPARE((*annot)->subType(), Poppler::Annotation::AText);
      auto textAnnot = static_cast<Poppler::TextAnnotation*>(*annot);
      QCOMPARE(textAnnot->contents(), contents);
      QCOMPARE(textAnnot->textFont().pointSize(), testFont.pointSize());
      QCOMPARE(static_cast<int>(textAnnot->textColor().spec()), static_cast<int>(color.spec()));
      QCOMPARE(textAnnot->textColor(), color);
      if (annot != annots.constEnd())
          ++annot;
    }
  }
}

static std::multimap<double, QRgb> colorRatios(const QImage& img) {
  const int nrOfPixel = img.size().width() * img.size().height();
  const QRgb* px = (QRgb*)img.constBits();
  std::map<QRgb, int> counts;
  std::multimap<double, QRgb> ratiosSorted;

  for (int i = 0; i < nrOfPixel; i++) {
    counts[px[i]]++;
  }
  std::transform(counts.begin(), counts.end(), std::inserter(ratiosSorted, ratiosSorted.begin()), [nrOfPixel](const std::pair<QRgb,double> &p){
    return std::pair<double, QRgb>((double)p.second / nrOfPixel, p.first);
  });
  return ratiosSorted;
}

static QRgb applyOpacityPremultiplied(const QColor& color, double opacity) {
  return QColor::fromRgbF(color.redF() * opacity, color.greenF() * opacity, color.blueF() * opacity, color.alphaF() * opacity).rgba();
}

void TestAnnotations::checkRenderToImage_data() {
  QTest::addColumn<std::shared_ptr<Poppler::Document>>("doc");
  QTest::addColumn<std::shared_ptr<Poppler::Page>>("page");
  QTest::addColumn<std::shared_ptr<Poppler::Annotation>>("annot");
  QTest::addColumn<double>("dpiX");
  QTest::addColumn<double>("dpiY");
  QTest::addColumn<Poppler::Document::RenderBackend>("renderBackend");
  QTest::addColumn<QSize>("expectedSize");
  QTest::addColumn<QList<ColorWithRatio>>("mostOccurringColors");

  const QString contents{"foobar"};
  const double dpiX = 100;
  const double dpiY = 100;
  const double unitsSizeInch = 1/72.;

  std::shared_ptr<Poppler::Document> doc{Poppler::Document::load(TESTDATADIR "/unittestcases/UseNone.pdf")};
  Q_ASSERT(doc.get());
  std::shared_ptr<Poppler::Page> page{doc->page(0)};
  Q_ASSERT(page.get());

  const QSize expectedSize{(int)(dpiX * page->pageSizeF().width() * unitsSizeInch), (int)(dpiY * page->pageSizeF().height() * unitsSizeInch)};
  Poppler::Annotation::Style s;
  s.setWidth(1);

  /* opaque FreeText */
  {
    auto annot = std::make_shared<Poppler::TextAnnotation>(Poppler::TextAnnotation::InPlace);
    annot->setBoundary(QRectF(0.0, 0.0, 1.0, 1.0));
    annot->setContents(contents);
    s.setColor(QColor{12, 34, 46, 255});
    s.setWidth(10);
    s.setOpacity(1.);
    annot->setStyle(s);
    QTest::newRow("FreeText Arthur opaque") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::ArthurBackend << expectedSize
        << QList<ColorWithRatio>{ {s.color().rgba(), 0.94}, {QColor(Qt::black).rgba(), 0.05} };
    QTest::newRow("FreeText Splash opaque") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::SplashBackend << expectedSize
        << QList<ColorWithRatio>{ {s.color().rgba(), 0.94}, {QColor(Qt::black).rgba(), 0.05} };
  }

  /* non-opaque FreeText */
  {
    auto annot = std::make_shared<Poppler::TextAnnotation>(Poppler::TextAnnotation::InPlace);
    annot->setBoundary(QRectF(0.0, 0.0, 1.0, 1.0));
    annot->setContents(contents);
    s.setColor(QColor{12, 34, 46, 255});
    s.setWidth(10);
    s.setOpacity(0.5);
    annot->setStyle(s);
    /* opacity doesn't work with Arthur atm */
    QTest::newRow("FreeText Splash non-opauque") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::SplashBackend << expectedSize
        << QList<ColorWithRatio>{ {applyOpacityPremultiplied(s.color().rgba(), s.opacity()), 0.94}, {applyOpacityPremultiplied(QColor(Qt::black), s.opacity()), 0.05} };
  }

  /* FreeText with transparent background */
  {
    auto annot = std::make_shared<Poppler::TextAnnotation>(Poppler::TextAnnotation::InPlace);
    annot->setBoundary(QRectF(0.0, 0.0, 1.0, 1.0));
    annot->setContents(contents);
    s.setColor(Qt::transparent);
    s.setWidth(10);
    s.setOpacity(1.);
    annot->setStyle(s);
    /* transparency doesn't work with Arthur atm */
    QTest::newRow("FreeText Splash transparent background") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::SplashBackend << expectedSize
        << QList<ColorWithRatio>{ {s.color().rgba(), 0.94}, {QColor(Qt::black).rgba(), 0.05} };
  }

  /* Text (aka popup note) */
  {
    auto annot = std::make_shared<Poppler::TextAnnotation>(Poppler::TextAnnotation::Linked);
    annot->setContents(contents);
    s.setColor(QColor{12, 34, 46, 255});
    annot->setStyle(s);

    /* Note: Currently, the icons are hard coded to 24x24pts. They are not scaled into the boundary given by Annotation::setBoundary().
     * I don't believe that's correct. But that's the way it is for now, so let's construct a boundary fits around the icon. */
    annot->setBoundary(QRectF(0.0, 0.0, 24./page->pageSizeF().width(), 24./page->pageSizeF().height()));
    const QSize expectedSize24pts{(int)(dpiX * 24. * unitsSizeInch), (int)(dpiY * 24. * unitsSizeInch)};

    QTest::newRow("Text Arthur") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::ArthurBackend << expectedSize24pts
        << QList<ColorWithRatio>{ {s.color().rgba(), 0.61}, {QColor(186, 189, 182, 255).rgba(), 0.25} };
    QTest::newRow("Text Splash") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::SplashBackend << expectedSize24pts
        << QList<ColorWithRatio>{ {s.color().rgba(), 0.54}, {QColor(186, 189, 182, 255).rgba(), 0.35} };
  }

  /* Circle (ellipse) */
  {
    auto annot = std::make_shared<Poppler::GeomAnnotation>();
    annot->setGeomType(Poppler::GeomAnnotation::InscribedCircle);
    annot->setContents(contents);
    annot->setBoundary(QRectF(0.0, 0.0, 1.0, 1.0));
    annot->setContents(contents);
    s.setColor(QColor{12, 34, 46, 255});
    s.setWidth(10);
    s.setOpacity(1.);
    annot->setStyle(s);
    annot->setGeomInnerColor(QColor{45, 56, 67, 255});
    QTest::newRow("Circle Arthur") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::ArthurBackend << expectedSize
        << QList<ColorWithRatio>{ {annot->geomInnerColor().rgba(), 0.72}, {QColor(255,255,255,255).rgba(), 0.23}, {s.color().rgba(), 0.04} };
    QTest::newRow("Circle Splash") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::SplashBackend << expectedSize
        << QList<ColorWithRatio>{ {annot->geomInnerColor().rgba(), 0.72}, {QColor(0,0,0,0).rgba(), 0.23}, {s.color().rgba(), 0.04} };
  }

  /* Ink */
  {
    auto annot = std::make_shared<Poppler::InkAnnotation>();
    annot->setBoundary(QRectF(0.0, 0.0, 1.0, 1.0));
    annot->setContents(contents);
    s.setColor(QColor{12, 34, 46, 255});
    s.setWidth(10);
    s.setOpacity(1.);
    annot->setStyle(s);
    annot->setInkPaths( QList<QLinkedList<QPointF>>{{QPointF{0.,0.}, QPointF{1.,1.}}});
    QTest::newRow("Ink Arthur") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::ArthurBackend << expectedSize
        << QList<ColorWithRatio>{ {QColor(255,255,255,255).rgba(), 0.97}, {s.color().rgba(), 0.02} };
    QTest::newRow("Ink Splash") << doc << page << static_cast<std::shared_ptr<Poppler::Annotation>>(annot)
        << dpiX << dpiY << Poppler::Document::SplashBackend << expectedSize
        << QList<ColorWithRatio>{ {QColor(0,0,0,0).rgba(), 0.97}, {s.color().rgba(), 0.02} };
  }
}

void TestAnnotations::checkRenderToImage()
{
  QFETCH(std::shared_ptr<Poppler::Document>, doc);
  QFETCH(std::shared_ptr<Poppler::Page>, page);
  QFETCH(std::shared_ptr<Poppler::Annotation>, annot);
  QFETCH(double, dpiX);
  QFETCH(double, dpiY);
  QFETCH(Poppler::Document::RenderBackend, renderBackend);
  QFETCH(QSize, expectedSize);
  QFETCH(QList<ColorWithRatio>, mostOccurringColors);

  doc->setRenderBackend(renderBackend);
  page->addAnnotation(annot.get());
  QImage renderedImage = annot->renderToImage(dpiX, dpiY);

  const auto actualColorRatios = colorRatios(renderedImage);
  QCOMPARE(renderedImage.size(), expectedSize);
  auto itRatiosSorted = actualColorRatios.crbegin();
  for (const auto& expectedColorRatio: mostOccurringColors) {
    QCOMPARE( itRatiosSorted->second, expectedColorRatio.color );
    QVERIFY( itRatiosSorted->first >= expectedColorRatio.ratio && itRatiosSorted->first <= 1. );
    itRatiosSorted++;
  }

#ifdef VERIFY_IMAGES_MANUALLY
  renderedImage.save(QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      QDir::separator() + QTest::currentTestFunction() + QTest::currentDataTag()) + ".png");
#endif
}

QTEST_MAIN(TestAnnotations)

#include "check_annotations.moc"
