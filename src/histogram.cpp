/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QMenu>
#include <QTime>
#include "histogramgenerator.h"
#include "histogram.h"
#include "renderer.h"

Histogram::Histogram(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    AbstractScopeWidget(projMonitor, clipMonitor, parent)
{
    ui = new Ui::Histogram_UI();
    ui->setupUi(this);

    ui->cbY->setChecked(true);
    ui->cbR->setChecked(true);
    ui->cbG->setChecked(true);
    ui->cbB->setChecked(true);


    m_aUnscaled = new QAction(i18n("Unscaled"), this);
    m_aUnscaled->setCheckable(true);
    m_menu->addSeparator();
    m_menu->addAction(m_aUnscaled);

    bool b = true;
    b &= connect(ui->cbY, SIGNAL(toggled(bool)), this, SLOT(forceUpdateScope()));
    b &= connect(ui->cbR, SIGNAL(toggled(bool)), this, SLOT(forceUpdateScope()));
    b &= connect(ui->cbG, SIGNAL(toggled(bool)), this, SLOT(forceUpdateScope()));
    b &= connect(ui->cbB, SIGNAL(toggled(bool)), this, SLOT(forceUpdateScope()));
    b &= connect(m_aUnscaled, SIGNAL(toggled(bool)), this, SLOT(forceUpdateScope()));
    Q_ASSERT(b);

}

Histogram::~Histogram()
{
    delete ui;
    delete m_aUnscaled;
}

QString Histogram::widgetName() const { return QString("Histogram"); }

bool Histogram::isHUDDependingOnInput() const { return false; }
bool Histogram::isScopeDependingOnInput() const { return true; }
bool Histogram::isBackgroundDependingOnInput() const { return false; }

QRect Histogram::scopeRect()
{
    qDebug() << "According to the spacer, the top left point is " << ui->verticalSpacer->geometry().x() << "/" << ui->verticalSpacer->geometry().y();
    QPoint topleft(offset, offset+ ui->verticalSpacer->geometry().y());
    return QRect(topleft, this->rect().size() - QSize(topleft.x() + offset, topleft.y() + offset));
}

QImage Histogram::renderHUD(uint)
{
    emit signalHUDRenderingFinished(0, 1);
    return QImage();
}
QImage Histogram::renderScope(uint accelFactor)
{
    QTime start = QTime::currentTime();
    start.start();

    const int componentFlags =   (ui->cbY->isChecked() ? 1 : 0) * HistogramGenerator::ComponentY
                               | (ui->cbR->isChecked() ? 1 : 0) * HistogramGenerator::ComponentR
                               | (ui->cbG->isChecked() ? 1 : 0) * HistogramGenerator::ComponentG
                               | (ui->cbB->isChecked() ? 1 : 0) * HistogramGenerator::ComponentB;

    QImage histogram = m_histogramGenerator->calculateHistogram(m_scopeRect.size(), m_activeRender->extractFrame(m_activeRender->seekFramePosition()),
                                                       componentFlags, m_aUnscaled->isChecked(), accelFactor);

    emit signalScopeRenderingFinished(start.elapsed(), accelFactor);
    return histogram;
}
QImage Histogram::renderBackground(uint)
{
    emit signalBackgroundRenderingFinished(0, 1);
    return QImage();
}