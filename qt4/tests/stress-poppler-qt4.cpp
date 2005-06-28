#include <iostream>
#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <ctype.h>

#define UNSTABLE_POPPLER_QT4
#include <poppler-qt4.h>

int main( int argc, char **argv )
{
    QApplication a( argc, argv );               // QApplication required!

    Q_UNUSED( argc );
    Q_UNUSED( argv );

    QTime t;
    t.start();
    QDir dbDir( QString( "./pdfdb" ) );
    if ( !dbDir.exists() ) {
	qWarning() << "Database directory does not exist";
    }

    QStringList excludeSubDirs;
    excludeSubDirs << "000048" << "000607";

    foreach ( QString subdir, dbDir.entryList(QStringList() << "0000*", QDir::Dirs) ) {
	if ( excludeSubDirs.contains(subdir) ) {
	    // then skip it
	} else {
	    QString path = "./pdfdb/" + subdir + "/data.pdf";
	    std::cout <<"Doing " << path.toLatin1().data() << " :";
	    Poppler::Document *doc = Poppler::Document::load( path );
	    if (!doc) {
		qWarning() << "doc not loaded";
	    } else {
		doc->pdfVersion();
		doc->info("Title");
		doc->info("Subject");
		doc->info("Author");
		doc->info("Keywords");
		doc->info("Creator");
		doc->info("Producer");
		doc->date("CreationDate").toString();
		doc->date("ModDate").toString();
		doc->numPages();
		doc->isLinearized();
		doc->isEncrypted();
		doc->okToPrint();
		doc->okToCopy();
		doc->okToChange();
		doc->okToAddNotes();
		doc->pageMode();

		QPixmap *pixmap = new QPixmap;
		for( int index = 0; index < doc->numPages(); ++index ) {
		    Poppler::Page *page = doc->page( index );
		    page->renderToPixmap(&pixmap, page->pageSize());
		    page->pageSize();
		    page->orientation();
		    std::cout << ".";
		    std::cout.flush();
		}
		std::cout << std::endl;
	    }
	}
    }

    std::cout << "Elapsed time: " << (t.elapsed()/1000) << std::endl;

}
