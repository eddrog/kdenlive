/***************************************************************************
 *   Copyright (C) 2012 by Ed Rogalsky (ed.rogalsky@googlemail.com)        *
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

#include <stdio.h>
#include <QCoreApplication>
#include <QStringList>
#include <QString>
#include <QUrl>
#include <QtDebug>

#include <mlt++/Mlt.h>
#include <jack/jack.h>
#include <iostream>
#include <string>
#include <signal.h>

Mlt::Consumer *pc;
bool volatile done;

void catcher(int sig)
{
//	if (pc != NULL)
//	{
//		printf ("consumer stopping ...");
//		pc->stop();
//		printf ("consumer stopped ...");
//	}

	done = true;

	printf ("Caught signal %d\n", sig);
	fflush(stdout);
}

int main(int argc, char **argv)
{
//    QCoreApplication app(argc, argv);
//    QStringList args = app.arguments();
    Mlt::Filter *mltFilterJack;
    Mlt::Profile *mltProfile = new Mlt::Profile();
	Mlt::Consumer *c;
	Mlt::Producer *p;
	Mlt::Producer *m_blackClip;
	Mlt::Properties *sdlProps;
	Mlt::Properties *jackProps;
    std::string mystr;

    done = false;
	signal(SIGINT, catcher);

	/* init mlt factory */
    Mlt::Factory::init();

    /* create the mlt jack filter */
//    mltFilterJack = new Mlt::Filter (*mltProfile, "jackrack", NULL);
//    std::cout << "jack filter created ..." << "\n";

    /**/
    m_blackClip = new Mlt::Producer(*mltProfile, "colour", "black");
    m_blackClip->set("id", "black");
    m_blackClip->set("mlt_type", "producer");


    Mlt::Playlist list;
    for ( int i = 1; i < argc; i ++ )
    {
    	p = new Mlt::Producer( *mltProfile, argv[i] );
        if ( p->is_valid( ) )
            list.append( *p );
    }

    /* some debug */
    std::cout << "playlist added ..." << "\n";

    /* create new consumer */
    c = new Mlt::Consumer ( *mltProfile, "multi", NULL );
    /* create the sdl properties instance */
    sdlProps = new Mlt::Properties();
    jackProps = new Mlt::Properties();

    // set comsumer properties
//	c->set("audio_off", 1);
    c->set("terminate_on_pause", 0);
    c->set("real_time", 1);

    /* init the sdl props */
	sdlProps->set("mlt_service", "sdl");
	sdlProps->set("audio_off", 1);
//	sdlProps->set("real_time", 1);
	sdlProps->set("rescale", "nearest");
	sdlProps->set("resize", 1);
//	sdlProps->set("terminate_on_pause", 0);

	/* init the jack props */
	jackProps->set("mlt_service", "jack");
//	jackProps->set("real_time", 1);
//	jackProps->set("terminate_on_pause", 0);
//	jackProps->set("audio_off", 0);
	/* append props to multi consumer */
	c->set("0", (void*) sdlProps->get_properties(), 0 , (mlt_destructor) NULL, NULL);
	c->set("1", (void*) jackProps->get_properties(), 0 , (mlt_destructor) NULL, NULL);
//	c->set("1", "jack");

	// set filter properties
//	mltFilterJack->set("out_1", "system:playback_1");
//	mltFilterJack->set("out_2", "system:playback_2");

//    pc = c;
    std::cout << "consumer created ..." << "\n";

//    c->attach(*mltFilterJack);
//    std::cout << "filter attached ..." << "\n";

    c->connect( *p );
//    c->connect( *m_blackClip );
    std::cout << "producer connected to consumer ..." << "\n";
    c->start();
//    c->stop();
//    Mlt::Consumer sdl((mlt_consumer)c->get_data("0.consumer"));
//    sdl.set("audio_off", 1);
//    c->start();

    std::cout << "consumer started ..." << "\n";

    while ( done == false)
    {
        std::cout << "waiting for stdin ..." << "\n";
    	getline (std::cin, mystr);
    	if (mystr == "s")
    	{
    		done = true;
    	}
    	else if (mystr == "p")
    	{
            p->set_speed(0.0);
            c->set("refresh", 0);
            p->seek(c->position());
    	}
    	else
    	{
            if (c->is_stopped())
            {
                c->start();
            }
            p->set_speed(1.0);
            c->set("refresh", 1);
    	}
    }

    std::cout << "consumer stopped ..." << "\n";

    if (!c->is_stopped())
    {
    	c->stop();
    }

    if(mltProfile != NULL)
    {
        std::cout << "profile destroyed ..." << "\n";
    	delete mltProfile;
    }

    if(p != NULL)
    {
        std::cout << "producer destroyed ..." << "\n";
    	delete p;
    }

    if(c != NULL)
    {
        std::cout << "consumer destroyed ..." << "\n";
    	delete c;
    }
    if (sdlProps != NULL)
    	delete sdlProps;
    	delete jackProps;

//    if(mltFilterJack != NULL)
//	{
//        std::cout << "filter destroyed ..." << "\n";
//		delete mltFilterJack;
//	}

//    /* shutdown mlt factory*/
//    Mlt::Factory::close();

    /* return */
    return 0;
}
