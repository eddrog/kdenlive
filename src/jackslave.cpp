/***************************************************************************
 *   Copyright (C) 2012 by Ed Rogalsky (ed.rogalsky@gmail.com)             *
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

#include "jackslave.h"

JackSlave::JackSlave(Mlt::Profile * profile) :
	m_isJackActive(false)
{
	if (m_isJackActive == false) {
		// create jackrack filter using the factory
		m_mltFilterJack = new Mlt::Filter(*profile, "jackrack", NULL);

		if(m_mltFilterJack != NULL && m_mltFilterJack->is_valid()) {
			m_isJackActive = true;
		}
	}
}

JackSlave& JackSlave::singleton(Mlt::Profile * profile)
{
    static JackSlave* instance = 0;

    if (!instance) {
    	instance = new JackSlave(profile);
    }

    return *instance;
}

JackSlave::~JackSlave()
{
	// TODO Auto-generated destructor stub
}
