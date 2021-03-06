/***************************************************************************
 *   Copyright (C) 2012 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef ABSTRACTCOLLAPSIBLEWIDGET_H
#define ABSTRACTCOLLAPSIBLEWIDGET_H

#include "ui_collapsiblewidget_ui.h"

#include <QWidget>
#include <QDomElement>

class AbstractCollapsibleWidget : public QWidget, public Ui::CollapsibleWidget_UI
{
    Q_OBJECT

public:
    AbstractCollapsibleWidget(QWidget * parent = 0);
    virtual void setActive(bool activate) = 0;
    virtual bool isGroup() const = 0;
    
signals:
    void addEffect(QDomElement e);
    /** @brief Move effects in the stack one step up or down. */
    void changeEffectPosition(QList <int>, bool upwards);
    /** @brief Move effects in the stack. */
    void moveEffect(QList <int> current_pos, int new_pos, int groupIndex, QString groupName);
    /** @brief An effect was saved, trigger effect list reload. */
    void reloadEffects();
  
};

#endif
