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


#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>
#include <KLocale>

#include "gentime.h"
#include "monitor.h"

Monitor::Monitor(QString name, MonitorManager *manager, QWidget *parent)
    : QWidget(parent), render(NULL), m_monitorManager(manager), m_name(name), m_isActive(false)
{
  ui.setupUi(this);
  m_scale = 1;
  m_ruler = new SmallRuler();
  QVBoxLayout *layout = new QVBoxLayout;
  layout->addWidget( m_ruler);
  ui.ruler_frame->setLayout( layout );
  //m_ruler->setPixelPerMark(3);
  m_playIcon = KIcon("media-playback-start");
  m_pauseIcon = KIcon("media-playback-pause");
  ui.button_play->setIcon(m_playIcon);
  ui.button_rew1->setIcon(KIcon("media-skip-backward"));
  ui.button_rew->setIcon(KIcon("media-seek-backward"));
  ui.button_fwd1->setIcon(KIcon("media-skip-forward"));
  ui.button_fwd->setIcon(KIcon("media-seek-forward"));
  connect(m_ruler, SIGNAL(seekRenderer(int)), this, SLOT(slotSeek(int)));
  connect(ui.button_rew, SIGNAL(clicked()), this, SLOT(slotRewind()));
  connect(ui.button_rew1, SIGNAL(clicked()), this, SLOT(slotRewindOneFrame()));
  connect(ui.button_fwd, SIGNAL(clicked()), this, SLOT(slotForward()));
  connect(ui.button_fwd1, SIGNAL(clicked()), this, SLOT(slotForwardOneFrame()));
  connect(ui.button_play, SIGNAL(clicked()), this, SLOT(slotPlay()));
  //if ( render ) return;
  render = new Render(m_name, (int) ui.video_frame->winId(), -1, this);
  connect(render, SIGNAL(durationChanged(int)), this, SLOT(adjustRulerSize(int)));
  connect(render, SIGNAL(rendererPosition(int)), this, SLOT(seekCursor(int)));
  connect(render, SIGNAL(rendererStopped(int)), this, SLOT(rendererStopped(int)));
  if (name != "clip") {
    connect(render, SIGNAL(rendererPosition(int)), this, SIGNAL(renderPosition(int)));
    connect(render, SIGNAL(durationChanged(int)), this, SIGNAL(durationChanged(int)));
  }
  //render->createVideoXWindow(ui.video_frame->winId(), -1);
  int width = m_ruler->width();
  m_ruler->setLength(width);
  m_ruler->setMaximum(width);
  m_length = 0;

  kDebug()<<"/////// BUILDING MONITOR, ID: "<<ui.video_frame->winId();
}

// virtual
void Monitor::mousePressEvent ( QMouseEvent * event )
{
  slotPlay();
}

// virtual
void Monitor::wheelEvent ( QWheelEvent * event )
{
  if (event->delta() > 0) slotForwardOneFrame();
  else slotRewindOneFrame();
}

void Monitor::activateMonitor()
{
  if (!m_isActive) m_monitorManager->activateMonitor(m_name);
}

void Monitor::slotSeek(int pos)
{
  if (!m_isActive) m_monitorManager->activateMonitor(m_name);
  if ( render == NULL ) return;
  int realPos = ((double) pos) / m_scale;
  render->seekToFrame(realPos);
  m_position = realPos;
  emit renderPosition(m_position);
  ui.monitor_time->setText(m_monitorManager->timecode().getTimecodeFromFrames(m_position));
}

void Monitor::slotRewind()
{
  if (!m_isActive) m_monitorManager->activateMonitor(m_name);
  double speed = render->playSpeed();
  if ( speed >= 0 ) render->play(-2);
  else render->play(speed * 2);
  ui.button_play->setChecked(true);
  ui.button_play->setIcon(m_pauseIcon);
}

void Monitor::slotForward()
{
  if (!m_isActive) m_monitorManager->activateMonitor(m_name);
  double speed = render->playSpeed();
  if ( speed <= 1 ) render->play(2);
  else render->play(speed * 2);
  ui.button_play->setChecked(true);
  ui.button_play->setIcon(m_pauseIcon);
}

void Monitor::slotRewindOneFrame()
{
  if (!m_isActive) m_monitorManager->activateMonitor(m_name);
  render->play(0);
  if (m_position < 1) return;
  m_position--;
  render->seekToFrame(m_position);
  emit renderPosition(m_position);
  ui.monitor_time->setText(m_monitorManager->timecode().getTimecodeFromFrames(m_position));
}

void Monitor::slotForwardOneFrame()
{
  if (!m_isActive) m_monitorManager->activateMonitor(m_name);
  render->play(0);
  if (m_position >= m_length) return;
  m_position++;
  render->seekToFrame(m_position);
  emit renderPosition(m_position);
  ui.monitor_time->setText(m_monitorManager->timecode().getTimecodeFromFrames(m_position));
}

void Monitor::seekCursor(int pos)
{
  if (!m_isActive) m_monitorManager->activateMonitor(m_name);
  int rulerPos = (int) (pos * m_scale);
  m_position = pos;
  ui.monitor_time->setText(m_monitorManager->timecode().getTimecodeFromFrames(pos));
  kDebug()<<"seek: "<<pos<<", scale: "<<m_scale;
  m_ruler->slotNewValue(rulerPos);
}

void Monitor::rendererStopped(int pos)
{
  int rulerPos = (int) (pos * m_scale);
  m_ruler->slotNewValue(rulerPos);
  m_position = pos;
  ui.monitor_time->setText(m_monitorManager->timecode().getTimecodeFromFrames(pos));
  ui.button_play->setChecked(false);
  ui.button_play->setIcon(m_playIcon);
}

void Monitor::initMonitor()
{
  kDebug()<<"/////// INITING MONITOR, ID: "<<ui.video_frame->winId();

}

// virtual
void Monitor::resizeEvent ( QResizeEvent * event )
{
  adjustRulerSize(-1);
  if (render) render->askForRefresh();
}

void Monitor::adjustRulerSize(int length)
{
  int width = m_ruler->width();
  m_ruler->setLength(width);
  if (length > 0) m_length = length;
  m_scale = (double) width / m_length;
  if (m_scale == 0) m_scale = 1;
  kDebug()<<"RULER WIDT: "<<width<<", RENDER LENGT: "<<m_length<<", SCALE: "<<m_scale;
  m_ruler->setPixelPerMark(m_scale);
  m_ruler->setMaximum(width);
  //m_ruler->setLength(length);
}

void Monitor::stop()
{
  m_isActive = false;
  if (render) render->stop();
}

void Monitor::start()
{
  m_isActive = true;
  if (render) render->start();
}

void Monitor::refreshMonitor(bool visible)
{
  if (visible && render) render->askForRefresh();
}

void Monitor::slotPlay()
{
  if ( render == NULL ) return;
  if (!m_isActive) m_monitorManager->activateMonitor(m_name);
  render->switchPlay();
  ui.button_play->setChecked(true);
  ui.button_play->setIcon(m_pauseIcon);
}

void Monitor::slotSetXml(const QDomElement &e)
{
    if ( render == NULL ) return;
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    westley.appendChild(e);
    render->setSceneList(doc, 0);
    m_ruler->slotNewValue(0);
    ui.monitor_time->setText("00:00:00:00");
    m_position = 0;
}


void Monitor::slotOpenFile(const QString &file)
{
    if ( render == NULL ) return;
    if (!m_isActive) m_monitorManager->activateMonitor(m_name);
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    QDomElement prod = doc.createElement("producer");
    westley.appendChild(prod);
    prod.setAttribute("mlt_service", "avformat");
    prod.setAttribute("resource", file);
    render->setSceneList(doc, 0);
}



#include "monitor.moc"
