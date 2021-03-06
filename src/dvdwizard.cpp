/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include "dvdwizard.h"
#include "dvdwizardvob.h"
#include "kdenlivesettings.h"
#include "profilesdialog.h"
#include "timecode.h"
#include "monitormanager.h"

#include <KStandardDirs>
#include <KLocale>
#include <KFileDialog>
#include <kmimetype.h>
#include <KIO/NetAccess>
#include <KMessageBox>

#include <QFile>
#include <QApplication>
#include <QTimer>
#include <QDomDocument>
#include <QMenu>
#include <QGridLayout>


DvdWizard::DvdWizard(MonitorManager *manager, const QString &url, QWidget *parent) :
        QWizard(parent)
        , m_dvdauthor(NULL)
        , m_mkiso(NULL)
        , m_vobitem(NULL)
        , m_burnMenu(new QMenu(this))
{
    setWindowTitle(i18n("DVD Wizard"));
    //setPixmap(QWizard::WatermarkPixmap, QPixmap(KStandardDirs::locate("appdata", "banner.png")));
    m_pageVob = new DvdWizardVob(this);
    m_pageVob->setTitle(i18n("Select Files For Your DVD"));
    addPage(m_pageVob);

    m_pageChapters = new DvdWizardChapters(manager, m_pageVob->dvdFormat(), this);
    m_pageChapters->setTitle(i18n("DVD Chapters"));
    addPage(m_pageChapters);
    
    if (!url.isEmpty()) m_pageVob->setUrl(url);
    connect(m_pageVob, SIGNAL(prepareMonitor()), this, SLOT(slotprepareMonitor()));



    m_pageMenu = new DvdWizardMenu(m_pageVob->dvdFormat(), this);
    m_pageMenu->setTitle(i18n("Create DVD Menu"));
    addPage(m_pageMenu);

    QWizardPage *page4 = new QWizardPage;
    page4->setTitle(i18n("Creating DVD Image"));
    m_status.setupUi(page4);
    m_status.error_box->setHidden(true);
    m_status.error_box->setTabBarHidden(true);
    m_status.tmp_folder->setUrl(KUrl(KdenliveSettings::currenttmpfolder()));
    m_status.tmp_folder->setMode(KFile::Directory | KFile::ExistingOnly);
    m_status.iso_image->setUrl(KUrl(QDir::homePath() + "/untitled.iso"));
    m_status.iso_image->setFilter("*.iso");
    m_status.iso_image->setMode(KFile::File);
    m_status.iso_image->fileDialog()->setOperationMode(KFileDialog::Saving);

#if KDE_IS_VERSION(4,7,0)
    m_isoMessage = new KMessageWidget;
    QGridLayout *s =  static_cast <QGridLayout*> (page4->layout());
    s->addWidget(m_isoMessage, 5, 0, 1, -1);
    m_isoMessage->hide();
#endif

    addPage(page4);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(slotPageChanged(int)));
    connect(m_status.button_start, SIGNAL(clicked()), this, SLOT(slotGenerate()));
    connect(m_status.button_abort, SIGNAL(clicked()), this, SLOT(slotAbort()));
    connect(m_status.button_preview, SIGNAL(clicked()), this, SLOT(slotPreview()));

    QString programName("k3b");
    QString exec = KStandardDirs::findExe(programName);
    if (!exec.isEmpty()) {
        //Add K3b action
        QAction *k3b = m_burnMenu->addAction(KIcon(programName), i18n("Burn with %1", programName), this, SLOT(slotBurn()));
        k3b->setData(exec);
    }
    programName = "brasero";
    exec = KStandardDirs::findExe(programName);
    if (!exec.isEmpty()) {
        //Add Brasero action
        QAction *brasero = m_burnMenu->addAction(KIcon(programName), i18n("Burn with %1", programName), this, SLOT(slotBurn()));
        brasero->setData(exec);
    }
    if (m_burnMenu->isEmpty()) m_burnMenu->addAction(i18n("No burning program found (K3b, Brasero)"));
    m_status.button_burn->setMenu(m_burnMenu);
    m_status.button_burn->setIcon(KIcon("tools-media-optical-burn"));
    m_status.button_burn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_status.button_preview->setIcon(KIcon("media-playback-start"));

    setButtonText(QWizard::CustomButton1, i18n("Load"));
    setButtonText(QWizard::CustomButton2, i18n("Save"));
    button(QWizard::CustomButton1)->setIcon(KIcon("document-open"));
    button(QWizard::CustomButton2)->setIcon(KIcon("document-save"));
    connect(button(QWizard::CustomButton1), SIGNAL(clicked()), this, SLOT(slotLoad()));
    connect(button(QWizard::CustomButton2), SIGNAL(clicked()), this, SLOT(slotSave()));
    setOption(QWizard::HaveCustomButton1, true);
    setOption(QWizard::HaveCustomButton2, true);
    QList<QWizard::WizardButton> layout;
    layout << QWizard::CustomButton1 << QWizard::CustomButton2 << QWizard::Stretch << QWizard::BackButton << QWizard::NextButton << QWizard::CancelButton << QWizard::FinishButton;
    setButtonLayout(layout);
}

DvdWizard::~DvdWizard()
{
    m_authorFile.remove();
    m_menuFile.remove();
    m_menuVobFile.remove();
    m_letterboxMovie.remove();
    m_menuImageBackground.remove();
    blockSignals(true);
    delete m_burnMenu;
    if (m_dvdauthor) {
        m_dvdauthor->blockSignals(true);
        m_dvdauthor->close();
        delete m_dvdauthor;
    }
    if (m_mkiso) {
        m_mkiso->blockSignals(true);
        m_mkiso->close();
        delete m_mkiso;
    }
}


void DvdWizard::slotPageChanged(int page)
{
    //kDebug() << "// PAGE CHGD: " << page << ", ID: " << visitedPages();
    if (page == 0) {
        // Update chapters that were modified in page 1
        m_pageChapters->stopMonitor();
        m_pageVob->updateChapters(m_pageChapters->chaptersData());
    } else if (page == 1) {
        m_pageChapters->setVobFiles(m_pageVob->dvdFormat(), m_pageVob->selectedUrls(), m_pageVob->durations(), m_pageVob->chapters());
	setTitleFormat(Qt::PlainText);
    } else if (page == 2) {
        m_pageChapters->stopMonitor();
        m_pageVob->updateChapters(m_pageChapters->chaptersData());
        m_pageMenu->setTargets(m_pageChapters->selectedTitles(), m_pageChapters->selectedTargets());
        m_pageMenu->changeProfile(m_pageVob->dvdFormat());
    }
}

void DvdWizard::slotprepareMonitor()
{
    m_pageChapters->createMonitor(m_pageVob->dvdFormat());
}

void DvdWizard::generateDvd()
{
#if KDE_IS_VERSION(4,7,0)
    m_isoMessage->animatedHide();
#endif
    m_status.error_box->setHidden(true);
    m_status.error_box->setCurrentIndex(0);
    m_status.error_box->setTabBarHidden(true);
    m_status.menu_file->clear();
    m_status.dvd_file->clear();

    m_selectedImage.setSuffix(".png");
    //m_selectedImage.setAutoRemove(false);
    m_selectedImage.open();
    
    m_selectedLetterImage.setSuffix(".png");
    //m_selectedLetterImage.setAutoRemove(false);
    m_selectedLetterImage.open();

    m_highlightedImage.setSuffix(".png");
    //m_highlightedImage.setAutoRemove(false);
    m_highlightedImage.open();
    
    m_highlightedLetterImage.setSuffix(".png");
    //m_highlightedLetterImage.setAutoRemove(false);
    m_highlightedLetterImage.open();

    m_menuImageBackground.setSuffix(".png");
    m_menuImageBackground.setAutoRemove(false);
    m_menuImageBackground.open();

    m_menuVideo.setSuffix(".vob");
    //m_menuVideo.setAutoRemove(false);
    m_menuVideo.open();

    m_menuFinalVideo.setSuffix(".vob");
    //m_menuFinalVideo.setAutoRemove(false);
    m_menuFinalVideo.open();

    m_letterboxMovie.close();
    m_letterboxMovie.setSuffix(".mpg");
    m_letterboxMovie.setAutoRemove(false);
    m_letterboxMovie.open();
    

    m_menuFile.close();
    m_menuFile.setSuffix(".xml");
    m_menuFile.setAutoRemove(false);
    m_menuFile.open();

    m_menuVobFile.close();
    m_menuVobFile.setSuffix(".mpg");
    m_menuVobFile.setAutoRemove(false);
    m_menuVobFile.open();

    m_authorFile.close();
    m_authorFile.setSuffix(".xml");
    m_authorFile.setAutoRemove(false);
    m_authorFile.open();

    QListWidgetItem *images =  m_status.job_progress->item(0);
    m_status.job_progress->setCurrentRow(0);
    images->setIcon(KIcon("system-run"));
    m_status.error_log->clear();
    // initialize html content
    m_status.error_log->setText("<html></html>");

    if (m_pageMenu->createMenu()) {
        m_pageMenu->createButtonImages(m_selectedImage.fileName(), m_highlightedImage.fileName(), false);
        m_pageMenu->createBackgroundImage(m_menuImageBackground.fileName(), false);
        images->setIcon(KIcon("dialog-ok"));
	connect(&m_menuJob, SIGNAL(finished (int, QProcess::ExitStatus)), this, SLOT(slotProcessMenuStatus(int, QProcess::ExitStatus)));
        //kDebug() << "/// STARTING MLT VOB CREATION: "<<m_selectedImage.fileName()<<m_menuImageBackground.fileName();
        if (!m_pageMenu->menuMovie()) {
            // create menu vob file
            m_vobitem =  m_status.job_progress->item(1);
            m_status.job_progress->setCurrentRow(1);
            m_vobitem->setIcon(KIcon("system-run"));

            QStringList args;
            args << "-profile" << m_pageVob->dvdProfile();
            args.append(m_menuImageBackground.fileName());
            args.append("in=0");
            args.append("out=100");
            args << "-consumer" << "avformat:" + m_menuVideo.fileName()<<"properties=DVD";
            m_menuJob.start(KdenliveSettings::rendererpath(), args);
        } else {
	    // Movie as menu background, do the compositing
	    m_vobitem =  m_status.job_progress->item(1);
            m_status.job_progress->setCurrentRow(1);
            m_vobitem->setIcon(KIcon("system-run"));

	    int menuLength = m_pageMenu->menuMovieLength();
	    if (menuLength == -1) {
		// menu movie is invalid
		errorMessage(i18n("Menu movie is invalid"));
		m_status.button_start->setEnabled(true);
                m_status.button_abort->setEnabled(false);
                return;
	    }
            QStringList args;
            args.append("-profile");
            args.append(m_pageVob->dvdProfile());
            args.append(m_pageMenu->menuMoviePath());
	    args << "-track" << m_menuImageBackground.fileName();
	    args << "out=" + QString::number(menuLength);
	    args << "-transition" << "composite" << "always_active=1";
            args << "-consumer" << "avformat:" + m_menuFinalVideo.fileName()<<"properties=DVD";
	    m_menuJob.start(KdenliveSettings::rendererpath(), args);
	    //kDebug()<<"// STARTING MENU JOB, image: "<<m_menuImageBackground.fileName()<<"\n-------------";
	}
    }
    else processDvdauthor();
}

void DvdWizard::processSpumux()
{
    kDebug() << "/// STARTING SPUMUX";
    QMap <QString, QRect> buttons = m_pageMenu->buttonsInfo();
    QStringList buttonsTarget;
    // create xml spumux file
    QListWidgetItem *spuitem =  m_status.job_progress->item(2);
    m_status.job_progress->setCurrentRow(2);
    spuitem->setIcon(KIcon("system-run"));
    QDomDocument doc;
    QDomElement sub = doc.createElement("subpictures");
    doc.appendChild(sub);
    QDomElement stream = doc.createElement("stream");
    sub.appendChild(stream);
    QDomElement spu = doc.createElement("spu");
    stream.appendChild(spu);
    spu.setAttribute("force", "yes");
    spu.setAttribute("start", "00:00:00.00");
    //spu.setAttribute("image", m_menuImage.fileName());
    spu.setAttribute("select", m_selectedImage.fileName());
    spu.setAttribute("highlight", m_highlightedImage.fileName());
    /*spu.setAttribute("autoorder", "rows");*/

    int max = buttons.count() - 1;
    int i = 0;
    QMapIterator<QString, QRect> it(buttons);
    while (it.hasNext()) {
        it.next();
        QDomElement but = doc.createElement("button");
        but.setAttribute("name", 'b' + QString::number(i));
        if (i < max) but.setAttribute("down", 'b' + QString::number(i + 1));
        else but.setAttribute("down", "b0");
        if (i > 0) but.setAttribute("up", 'b' + QString::number(i - 1));
        else but.setAttribute("up", 'b' + QString::number(max));
        QRect r = it.value();
        //int target = it.key();
        // TODO: solve play all button
        //if (target == 0) target = 1;

        // We need to make sure that the y coordinate is a multiple of 2, otherwise button may not be displayed
        buttonsTarget.append(it.key());
	int y0 = r.y();
	if (y0 % 2 == 1) y0++;
	int y1 = r.bottom();
	if (y1 % 2 == 1) y1--;
        but.setAttribute("x0", QString::number(r.x()));
        but.setAttribute("y0", QString::number(y0));
        but.setAttribute("x1", QString::number(r.right()));
        but.setAttribute("y1", QString::number(y1));
        spu.appendChild(but);
	i++;
    }

    QFile data(m_menuFile.fileName());
    if (data.open(QFile::WriteOnly)) {
	data.write(doc.toString().toUtf8());
    }
    data.close();

    //kDebug() << " SPUMUX DATA: " << doc.toString();

    QStringList args;
    args << "-s" << "0" << m_menuFile.fileName();
    //kDebug() << "SPM ARGS: " << args << m_menuVideo.fileName() << m_menuVobFile.fileName();

    QProcess spumux;
    QString menuMovieUrl;

#if QT_VERSION >= 0x040600
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("VIDEO_FORMAT", m_pageVob->dvdFormat() == PAL || m_pageVob->dvdFormat() == PAL_WIDE ? "PAL" : "NTSC");
    spumux.setProcessEnvironment(env);
#else
    QStringList env = QProcess::systemEnvironment();
    env << QString("VIDEO_FORMAT=") + QString(m_pageVob->dvdFormat() == PAL || m_pageVob->dvdFormat() == PAL_WIDE ? "PAL" : "NTSC");
    spumux.setEnvironment(env);
#endif
    
    if (m_pageMenu->menuMovie()) spumux.setStandardInputFile(m_menuFinalVideo.fileName());
    else spumux.setStandardInputFile(m_menuVideo.fileName());
    spumux.setStandardOutputFile(m_menuVobFile.fileName());
    spumux.start("spumux", args);
    if (spumux.waitForFinished()) {
        m_status.error_log->append(spumux.readAllStandardError());
        if (spumux.exitStatus() == QProcess::CrashExit) {
            //TODO: inform user via messagewidget after string freeze
            QByteArray result = spumux.readAllStandardError();
            spuitem->setIcon(KIcon("dialog-close"));
            m_status.error_log->append(result);
            m_status.error_box->setHidden(false);
            m_status.error_box->setTabBarHidden(false);
            m_status.menu_file->setPlainText(m_menuFile.readAll());
            m_status.dvd_file->setPlainText(m_authorFile.readAll());
            m_status.button_start->setEnabled(true);
            kDebug() << "/// RENDERING SPUMUX MENU crashed";
	    return;
	}
    } else {
        kDebug() << "/// RENDERING SPUMUX MENU timed out";
        errorMessage(i18n("Rendering job timed out"));
        spuitem->setIcon(KIcon("dialog-close"));
        m_status.error_log->append("<a name=\"result\" /><br /><strong>" + i18n("Menu job timed out"));
        m_status.error_log->scrollToAnchor("result");
        m_status.error_box->setHidden(false);
        m_status.error_box->setTabBarHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        m_status.button_start->setEnabled(true);
	return;
    }
    if (m_pageVob->dvdFormat() == PAL_WIDE || m_pageVob->dvdFormat() == NTSC_WIDE) {
	// Second step processing for 16:9 DVD, add letterbox stream
	m_pageMenu->createButtonImages(m_selectedLetterImage.fileName(), m_highlightedLetterImage.fileName(), true);
	buttons = m_pageMenu->buttonsInfo(true);
	
	QDomDocument docLetter;
	QDomElement subLetter = docLetter.createElement("subpictures");
	docLetter.appendChild(subLetter);
	QDomElement streamLetter = docLetter.createElement("stream");
	subLetter.appendChild(streamLetter);
	QDomElement spuLetter = docLetter.createElement("spu");
	streamLetter.appendChild(spuLetter);
	spuLetter.setAttribute("force", "yes");
	spuLetter.setAttribute("start", "00:00:00.00");
	spuLetter.setAttribute("select", m_selectedLetterImage.fileName());
	spuLetter.setAttribute("highlight", m_highlightedLetterImage.fileName());

	max = buttons.count() - 1;
	i = 0;
	QMapIterator<QString, QRect> it2(buttons);
	while (it2.hasNext()) {
	    it2.next();
	    QDomElement but = docLetter.createElement("button");
	    but.setAttribute("name", 'b' + QString::number(i));
	    if (i < max) but.setAttribute("down", 'b' + QString::number(i + 1));
	    else but.setAttribute("down", "b0");
	    if (i > 0) but.setAttribute("up", 'b' + QString::number(i - 1));
	    else but.setAttribute("up", 'b' + QString::number(max));
	    QRect r = it2.value();
	    // We need to make sure that the y coordinate is a multiple of 2, otherwise button may not be displayed
	    buttonsTarget.append(it2.key());
	    int y0 = r.y();
	    if (y0 % 2 == 1) y0++;
	    int y1 = r.bottom();
	    if (y1 % 2 == 1) y1--;
	    but.setAttribute("x0", QString::number(r.x()));
	    but.setAttribute("y0", QString::number(y0));
	    but.setAttribute("x1", QString::number(r.right()));
	    but.setAttribute("y1", QString::number(y1));
	    spuLetter.appendChild(but);
	    i++;
	}

	//kDebug() << " SPUMUX DATA: " << doc.toString();
    
	if (data.open(QFile::WriteOnly)) {
	    data.write(docLetter.toString().toUtf8());
	}
	data.close();
	spumux.setStandardInputFile(m_menuVobFile.fileName());
	spumux.setStandardOutputFile(m_letterboxMovie.fileName());
	args.clear();
	args << "-s" << "1" << m_menuFile.fileName();
	spumux.start("spumux", args);
	//kDebug() << "SPM ARGS LETTERBOX: " << args << m_menuVideo.fileName() << m_letterboxMovie.fileName();
	if (spumux.waitForFinished()) {
	    m_status.error_log->append(spumux.readAllStandardError());
	    if (spumux.exitStatus() == QProcess::CrashExit) {
		//TODO: inform user via messagewidget after string freeze
		QByteArray result = spumux.readAllStandardError();
		spuitem->setIcon(KIcon("dialog-close"));
		m_status.error_log->append(result);
		m_status.error_box->setHidden(false);
		m_status.error_box->setTabBarHidden(false);
		m_status.menu_file->setPlainText(m_menuFile.readAll());
		m_status.dvd_file->setPlainText(m_authorFile.readAll());
		m_status.button_start->setEnabled(true);
		kDebug() << "/// RENDERING SPUMUX MENU crashed";
		return;
	    }
	} else {
	    kDebug() << "/// RENDERING SPUMUX MENU timed out";
	    errorMessage(i18n("Rendering job timed out"));
	    spuitem->setIcon(KIcon("dialog-close"));
	    m_status.error_log->append("<a name=\"result\" /><br /><strong>" + i18n("Menu job timed out"));
	    m_status.error_log->scrollToAnchor("result");
	    m_status.error_box->setHidden(false);
	    m_status.error_box->setTabBarHidden(false);
	    m_status.menu_file->setPlainText(m_menuFile.readAll());
	    m_status.dvd_file->setPlainText(m_authorFile.readAll());
	    m_status.button_start->setEnabled(true);
	    return;
	}
	menuMovieUrl = m_letterboxMovie.fileName();
    }
    else menuMovieUrl = m_menuVobFile.fileName();

    spuitem->setIcon(KIcon("dialog-ok"));
    kDebug() << "/// DONE: " << menuMovieUrl;
    processDvdauthor(menuMovieUrl, buttons, buttonsTarget);
}

void DvdWizard::processDvdauthor(QString menuMovieUrl, QMap <QString, QRect> buttons, QStringList buttonsTarget)
{
    // create dvdauthor xml
    QListWidgetItem *authitem =  m_status.job_progress->item(3);
    m_status.job_progress->setCurrentRow(3);
    authitem->setIcon(KIcon("system-run"));
    KIO::NetAccess::mkdir(KUrl(m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"), this);

    QDomDocument dvddoc;
    QDomElement auth = dvddoc.createElement("dvdauthor");
    auth.setAttribute("dest", m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD");
    dvddoc.appendChild(auth);
    QDomElement vmgm = dvddoc.createElement("vmgm");
    auth.appendChild(vmgm);

    if (m_pageMenu->createMenu() && !m_pageVob->introMovie().isEmpty()) {
        // Use first movie in list as intro movie
        QDomElement menus = dvddoc.createElement("menus");
        vmgm.appendChild(menus);
        QDomElement pgc = dvddoc.createElement("pgc");
        pgc.setAttribute("entry", "title");
        menus.appendChild(pgc);
        QDomElement menuvob = dvddoc.createElement("vob");
        menuvob.setAttribute("file", m_pageVob->introMovie());
        pgc.appendChild(menuvob);
        QDomElement post = dvddoc.createElement("post");
        QDomText call = dvddoc.createTextNode("jump titleset 1 menu;");
        post.appendChild(call);
        pgc.appendChild(post);
    }
    QDomElement titleset = dvddoc.createElement("titleset");
    auth.appendChild(titleset);

    if (m_pageMenu->createMenu()) {

        // DVD main menu
        QDomElement menus = dvddoc.createElement("menus");
        titleset.appendChild(menus);
	
	QDomElement menuvideo = dvddoc.createElement("video");
	menus.appendChild(menuvideo);
	switch (m_pageVob->dvdFormat()) {
	    case PAL_WIDE:
		menuvideo.setAttribute("format", "pal");
		menuvideo.setAttribute("aspect", "16:9");
		menuvideo.setAttribute("widescreen", "nopanscan");
		break;
	    case NTSC_WIDE:
		menuvideo.setAttribute("format", "ntsc");
		menuvideo.setAttribute("aspect", "16:9");
		menuvideo.setAttribute("widescreen", "nopanscan");
		break;
	    case NTSC:
		menuvideo.setAttribute("format", "ntsc");
		menuvideo.setAttribute("aspect", "4:3");
		break;
	    default:
		menuvideo.setAttribute("format", "pal");
		menuvideo.setAttribute("aspect", "4:3");
		break;
	}

	
	if (m_pageVob->dvdFormat() == PAL_WIDE || m_pageVob->dvdFormat() == NTSC_WIDE) {
	    // Add letterbox stream info
	    QDomElement subpict = dvddoc.createElement("subpicture");
	    QDomElement stream = dvddoc.createElement("stream");
	    stream.setAttribute("id", "0");
	    stream.setAttribute("mode", "widescreen");
	    subpict.appendChild(stream);
	    QDomElement stream2 = dvddoc.createElement("stream");
	    stream2.setAttribute("id", "1");
	    stream2.setAttribute("mode", "letterbox");
	    subpict.appendChild(stream2);
	    menus.appendChild(subpict);
	}
        QDomElement pgc = dvddoc.createElement("pgc");
        pgc.setAttribute("entry", "root");
        menus.appendChild(pgc);
        QDomElement pre = dvddoc.createElement("pre");
        pgc.appendChild(pre);
        QDomText nametext = dvddoc.createTextNode("{g1 = 0;}");
        pre.appendChild(nametext);
	QDomElement menuvob = dvddoc.createElement("vob");
        menuvob.setAttribute("file", menuMovieUrl);
        pgc.appendChild(menuvob);
        for (int i = 0; i < buttons.count(); i++) {
            QDomElement button = dvddoc.createElement("button");
            button.setAttribute("name", 'b' + QString::number(i));
            nametext = dvddoc.createTextNode('{' + buttonsTarget.at(i) + ";}");
            button.appendChild(nametext);
            pgc.appendChild(button);
        }

        if (m_pageMenu->loopMovie()) {
            QDomElement menuloop = dvddoc.createElement("post");
            nametext = dvddoc.createTextNode("jump titleset 1 menu;");
            menuloop.appendChild(nametext);
            pgc.appendChild(menuloop);
        } else menuvob.setAttribute("pause", "inf");

    }

    QDomElement titles = dvddoc.createElement("titles");
    titleset.appendChild(titles);
    QDomElement video = dvddoc.createElement("video");
    titles.appendChild(video);
    switch (m_pageVob->dvdFormat()) {
	case PAL_WIDE:
	    video.setAttribute("format", "pal");
	    video.setAttribute("aspect", "16:9");
	    break;
	case NTSC_WIDE:
	    video.setAttribute("format", "ntsc");
	    video.setAttribute("aspect", "16:9");
	    break;
	case NTSC:
	    video.setAttribute("format", "ntsc");
	    video.setAttribute("aspect", "4:3");
	    break;
	default:
	    video.setAttribute("format", "pal");
	    video.setAttribute("aspect", "4:3");
	    break;
    }

    QDomElement pgc2;
    // Get list of clips
    QStringList voburls = m_pageVob->selectedUrls();

    for (int i = 0; i < voburls.count(); i++) {
        if (!voburls.at(i).isEmpty()) {
            // Add vob entry
            pgc2 = dvddoc.createElement("pgc");
            pgc2.setAttribute("pause", 0);
            titles.appendChild(pgc2);
            QDomElement vob = dvddoc.createElement("vob");
            vob.setAttribute("file", voburls.at(i));
            // Add chapters
            QStringList chaptersList = m_pageChapters->chapters(i);
            if (!chaptersList.isEmpty()) vob.setAttribute("chapters", chaptersList.join(","));

            pgc2.appendChild(vob);
            if (m_pageMenu->createMenu()) {
                QDomElement post = dvddoc.createElement("post");
                QDomText call;
                if (i == voburls.count() - 1) call = dvddoc.createTextNode("{g1 = 0; call menu;}");
                else {
                    call = dvddoc.createTextNode("{if ( g1 eq 999 ) { call menu; } jump title " + QString::number(i + 2).rightJustified(2, '0') + ";}");
                }
                post.appendChild(call);
                pgc2.appendChild(post);
            }
        }
    }


    QFile data2(m_authorFile.fileName());
    if (data2.open(QFile::WriteOnly)) {
        data2.write(dvddoc.toString().toUtf8());
    }
    data2.close();
    /*kDebug() << "------------------";
    kDebug() << dvddoc.toString();
    kDebug() << "------------------";*/

    QStringList args;
    args << "-x" << m_authorFile.fileName();
    kDebug() << "// DVDAUTH ARGS: " << args;
    if (m_dvdauthor) {
        m_dvdauthor->blockSignals(true);
        m_dvdauthor->close();
        delete m_dvdauthor;
        m_dvdauthor = NULL;
    }
    m_creationLog.clear();
    m_dvdauthor = new QProcess(this);
    // Set VIDEO_FORMAT variable (required by dvdauthor 0.7)
#if QT_VERSION >= 0x040600
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("VIDEO_FORMAT", m_pageVob->dvdFormat() == PAL || m_pageVob->dvdFormat() == PAL_WIDE ? "PAL" : "NTSC"); 
    m_dvdauthor->setProcessEnvironment(env);
#else
    QStringList env = QProcess::systemEnvironment();
    env << QString("VIDEO_FORMAT=") + QString(m_pageVob->dvdFormat() == PAL || m_pageVob->dvdFormat() == PAL_WIDE ? "PAL" : "NTSC");
    m_dvdauthor->setEnvironment(env);
#endif
    connect(m_dvdauthor, SIGNAL(finished(int , QProcess::ExitStatus)), this, SLOT(slotRenderFinished(int, QProcess::ExitStatus)));
    connect(m_dvdauthor, SIGNAL(readyReadStandardOutput()), this, SLOT(slotShowRenderInfo()));
    m_dvdauthor->setProcessChannelMode(QProcess::MergedChannels);
    m_dvdauthor->start("dvdauthor", args);
    m_status.button_abort->setEnabled(true);
    button(QWizard::FinishButton)->setEnabled(false);
}

void DvdWizard::slotProcessMenuStatus(int, QProcess::ExitStatus status)
{
    if (status == QProcess::CrashExit) {
	kDebug() << "/// RENDERING MENU vob crashed";
        errorMessage(i18n("Rendering menu crashed"));
        QByteArray result = m_menuJob.readAllStandardError();
        if (m_vobitem) m_vobitem->setIcon(KIcon("dialog-close"));
        m_status.error_log->append(result);
        m_status.error_box->setHidden(false);
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        return;
    }
    if (m_vobitem) m_vobitem->setIcon(KIcon("dialog-ok"));
    processSpumux();
}

void DvdWizard::slotShowRenderInfo()
{
    QString log = QString(m_dvdauthor->readAll());
    m_status.error_log->append(log);
    m_status.error_box->setHidden(false);
}

void DvdWizard::errorMessage(const QString &text) {
#if KDE_IS_VERSION(4,7,0)
    m_isoMessage->setText(text);
    m_isoMessage->setMessageType(KMessageWidget::Error);
    m_isoMessage->animatedShow();
#endif
}

void DvdWizard::infoMessage(const QString &text) {
#if KDE_IS_VERSION(4,7,0)
    m_isoMessage->setText(text);
    m_isoMessage->setMessageType(KMessageWidget::Positive);
    m_isoMessage->animatedShow();
#endif
}

void DvdWizard::slotRenderFinished(int exitCode, QProcess::ExitStatus status)
{
    QListWidgetItem *authitem =  m_status.job_progress->item(3);
    if (status == QProcess::CrashExit || exitCode != 0) {
        errorMessage(i18n("DVDAuthor process crashed"));
        QString result(m_dvdauthor->readAllStandardError());
        result.append("<a name=\"result\" /><br /><strong>");
        result.append(i18n("DVDAuthor process crashed.</strong><br />"));
        m_status.error_log->append(result);
        m_status.error_log->scrollToAnchor("result");
        m_status.error_box->setHidden(false);
        m_status.error_box->setTabBarHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        kDebug() << "DVDAuthor process crashed";
        authitem->setIcon(KIcon("dialog-close"));
        m_dvdauthor->close();
        delete m_dvdauthor;
        m_dvdauthor = NULL;
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        cleanup();
        button(QWizard::FinishButton)->setEnabled(true);
        return;
    }
    m_creationLog.append(m_dvdauthor->readAllStandardError());
    m_dvdauthor->close();
    delete m_dvdauthor;
    m_dvdauthor = NULL;

    // Check if DVD structure has the necessary infos
    if (!QFile::exists(m_status.tmp_folder->url().path() + "/DVD/VIDEO_TS/VIDEO_TS.IFO")) {
        errorMessage(i18n("DVD structure broken"));
        m_status.error_log->append(m_creationLog + "<a name=\"result\" /><br /><strong>" + i18n("DVD structure broken"));
        m_status.error_log->scrollToAnchor("result");
        m_status.error_box->setHidden(false);
        m_status.error_box->setTabBarHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        kDebug() << "DVDAuthor process crashed";
        authitem->setIcon(KIcon("dialog-close"));
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        cleanup();
        button(QWizard::FinishButton)->setEnabled(true);
        return;
    }
    authitem->setIcon(KIcon("dialog-ok"));
    QStringList args;
    args << "-dvd-video" << "-v" << "-o" << m_status.iso_image->url().path() << m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD";

    if (m_mkiso) {
        m_mkiso->blockSignals(true);
        m_mkiso->close();
        delete m_mkiso;
        m_mkiso = NULL;
    }
    m_mkiso = new QProcess(this);
    connect(m_mkiso, SIGNAL(finished(int , QProcess::ExitStatus)), this, SLOT(slotIsoFinished(int, QProcess::ExitStatus)));
    connect(m_mkiso, SIGNAL(readyReadStandardOutput()), this, SLOT(slotShowIsoInfo()));
    m_mkiso->setProcessChannelMode(QProcess::MergedChannels);
    QListWidgetItem *isoitem =  m_status.job_progress->item(4);
    m_status.job_progress->setCurrentRow(4);
    isoitem->setIcon(KIcon("system-run"));
    if (!KStandardDirs::findExe("genisoimage").isEmpty()) m_mkiso->start("genisoimage", args);
    else m_mkiso->start("mkisofs", args);

}

void DvdWizard::slotShowIsoInfo()
{
    QString log = QString(m_mkiso->readAll());
    m_status.error_log->append(log);
    m_status.error_box->setHidden(false);
}

void DvdWizard::slotIsoFinished(int exitCode, QProcess::ExitStatus status)
{
    button(QWizard::FinishButton)->setEnabled(true);
    QListWidgetItem *isoitem =  m_status.job_progress->item(4);
    if (status == QProcess::CrashExit || exitCode != 0) {
        errorMessage(i18n("ISO creation process crashed."));
        QString result(m_mkiso->readAllStandardError());
        result.append("<a name=\"result\" /><br /><strong>");
        result.append(i18n("ISO creation process crashed."));
        m_status.error_log->append(result);
        m_status.error_log->scrollToAnchor("result");
        m_status.error_box->setHidden(false);
        m_status.error_box->setTabBarHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        m_mkiso->close();
        delete m_mkiso;
        m_mkiso = NULL;
        cleanup();
        kDebug() << "Iso process crashed";
        isoitem->setIcon(KIcon("dialog-close"));
        m_status.button_start->setEnabled(true);
        m_status.button_abort->setEnabled(false);
        return;
    }

    m_creationLog.append(m_mkiso->readAllStandardError());
    delete m_mkiso;
    m_mkiso = NULL;
    m_status.button_start->setEnabled(true);
    m_status.button_abort->setEnabled(false);

    // Check if DVD iso is ok
    QFile iso(m_status.iso_image->url().path());
    if (!iso.exists() || iso.size() == 0) {
        if (iso.exists()) {
            KIO::NetAccess::del(m_status.iso_image->url(), this);
        }
        errorMessage(i18n("DVD ISO is broken"));
        m_status.error_log->append(m_creationLog + "<br /><a name=\"result\" /><strong>" + i18n("DVD ISO is broken") + "</strong>");
        m_status.error_log->scrollToAnchor("result");
        m_status.error_box->setHidden(false);
        m_status.error_box->setTabBarHidden(false);
        m_status.menu_file->setPlainText(m_menuFile.readAll());
        m_status.dvd_file->setPlainText(m_authorFile.readAll());
        isoitem->setIcon(KIcon("dialog-close"));
        cleanup();
        return;
    }

    isoitem->setIcon(KIcon("dialog-ok"));
    kDebug() << "ISO IMAGE " << m_status.iso_image->url().path() << " Successfully created";
    cleanup();
    kDebug() << m_creationLog;
    infoMessage(i18n("DVD ISO image %1 successfully created.", m_status.iso_image->url().path()));

    m_status.error_log->append("<a name=\"result\" /><strong>" + i18n("DVD ISO image %1 successfully created.", m_status.iso_image->url().path()) + "</strong>");
    m_status.error_log->scrollToAnchor("result");
    m_status.button_preview->setEnabled(true);
    m_status.button_burn->setEnabled(true);
    m_status.error_box->setHidden(false);
    //KMessageBox::information(this, i18n("DVD ISO image %1 successfully created.", m_status.iso_image->url().path()));

}


void DvdWizard::cleanup()
{
    KIO::NetAccess::del(KUrl(m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"), this);
}


void DvdWizard::slotPreview()
{
    QStringList programNames;
    programNames << "xine" << "vlc";
    QString exec;
    foreach(const QString &prog, programNames) {
	exec = KStandardDirs::findExe(prog);
	if (!exec.isEmpty()) {
	    break;
	}
    }
    if (exec.isEmpty()) {
	KMessageBox::sorry(this, i18n("Previewing requires one of these applications (%1)", programNames.join(",")));
    }
    else QProcess::startDetached(exec, QStringList() << "dvd://" + m_status.iso_image->url().path());
}

void DvdWizard::slotBurn()
{
    QAction *action = qobject_cast<QAction *>(sender());
    QString exec = action->data().toString();
    QStringList args;
    if (exec.endsWith("k3b")) args << "--image" << m_status.iso_image->url().path();
    else args << "--image=" + m_status.iso_image->url().path();
    QProcess::startDetached(exec, args);
}


void DvdWizard::slotGenerate()
{
    // clear job icons
    if ((m_dvdauthor && m_dvdauthor->state() != QProcess::NotRunning) || (m_mkiso && m_mkiso->state() != QProcess::NotRunning)) return;
    for (int i = 0; i < m_status.job_progress->count(); i++)
        m_status.job_progress->item(i)->setIcon(KIcon());
    QString warnMessage;
    if (KIO::NetAccess::exists(KUrl(m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"), KIO::NetAccess::SourceSide, this))
        warnMessage.append(i18n("Folder %1 already exists. Overwrite?\n", m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"));
    if (KIO::NetAccess::exists(KUrl(m_status.iso_image->url().path()), KIO::NetAccess::SourceSide, this))
        warnMessage.append(i18n("Image file %1 already exists. Overwrite?", m_status.iso_image->url().path()));

    if (warnMessage.isEmpty() || KMessageBox::questionYesNo(this, warnMessage) == KMessageBox::Yes) {
        KIO::NetAccess::del(KUrl(m_status.tmp_folder->url().path(KUrl::AddTrailingSlash) + "DVD"), this);
        QTimer::singleShot(300, this, SLOT(generateDvd()));
        m_status.button_preview->setEnabled(false);
        m_status.button_burn->setEnabled(false);
        m_status.job_progress->setEnabled(true);
        m_status.button_start->setEnabled(false);
    }
}

void DvdWizard::slotAbort()
{
    // clear job icons
    if (m_dvdauthor && m_dvdauthor->state() != QProcess::NotRunning) m_dvdauthor->terminate();
    else if (m_mkiso && m_mkiso->state() != QProcess::NotRunning) m_mkiso->terminate();
}

void DvdWizard::slotSave()
{
    KUrl url = KFileDialog::getSaveUrl(KUrl("kfiledialog:///projectfolder"), "*.kdvd", this, i18n("Save DVD Project"));
    if (url.isEmpty()) return;

    if (currentId() == 0) m_pageChapters->setVobFiles(m_pageVob->dvdFormat(), m_pageVob->selectedUrls(), m_pageVob->durations(), m_pageVob->chapters());

    QDomDocument doc;
    QDomElement dvdproject = doc.createElement("dvdproject");
    dvdproject.setAttribute("profile", m_pageVob->dvdProfile());
    dvdproject.setAttribute("tmp_folder", m_status.tmp_folder->url().path());
    dvdproject.setAttribute("iso_image", m_status.iso_image->url().path());
    dvdproject.setAttribute("intro_movie", m_pageVob->introMovie());

    doc.appendChild(dvdproject);
    QDomElement menu = m_pageMenu->toXml();
    if (!menu.isNull()) dvdproject.appendChild(doc.importNode(menu, true));
    QDomElement chaps = m_pageChapters->toXml();
    if (!chaps.isNull()) dvdproject.appendChild(doc.importNode(chaps, true));

    QFile file(url.path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        kWarning() << "//////  ERROR writing to file: " << url.path();
        KMessageBox::error(this, i18n("Cannot write to file %1", url.path()));
        return;
    }

    file.write(doc.toString().toUtf8());
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", url.path()));
    }
    file.close();
}


void DvdWizard::slotLoad()
{
    KUrl url = KFileDialog::getOpenUrl(KUrl("kfiledialog:///projectfolder"), "*.kdvd");
    if (url.isEmpty()) return;
    QDomDocument doc;
    QFile file(url.path());
    doc.setContent(&file, false);
    file.close();
    QDomElement dvdproject = doc.documentElement();
    if (dvdproject.tagName() != "dvdproject") {
        KMessageBox::error(this, i18n("File %1 is not a Kdenlive project file.", url.path()));
        return;
    }

    QString profile = dvdproject.attribute("profile");
    m_pageVob->setProfile(profile);
    m_pageVob->clear();
    m_status.tmp_folder->setUrl(KUrl(dvdproject.attribute("tmp_folder")));
    m_status.iso_image->setUrl(KUrl(dvdproject.attribute("iso_image")));
    QString intro = dvdproject.attribute("intro_movie");
    if (!intro.isEmpty()) {
	m_pageVob->slotAddVobFile(KUrl(intro));
	m_pageVob->setUseIntroMovie(true);
    }

    QDomNodeList vobs = doc.elementsByTagName("vob");
    for (int i = 0; i < vobs.count(); i++) {
        QDomElement e = vobs.at(i).toElement();
        m_pageVob->slotAddVobFile(KUrl(e.attribute("file")), e.attribute("chapters"));
    }
    m_pageMenu->loadXml(m_pageVob->dvdFormat(), dvdproject.firstChildElement("menu"));
}
