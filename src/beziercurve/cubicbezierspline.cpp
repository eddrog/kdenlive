/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "cubicbezierspline.h"


/** @brief For sorting a Bezier spline. Whether a is before b. */
static bool pointLessThan(const BPoint &a, const BPoint &b)
{
    return a.p.x() < b.p.x();
}

CubicBezierSpline::CubicBezierSpline(QObject* parent) :
        QObject(parent),
        m_validSpline(false),
        m_precision(100)
{
    m_points.append(BPoint(QPointF(0, 0), QPointF(0, 0), QPointF(.1, .1)));
    m_points.append(BPoint(QPointF(.9, .9), QPointF(1, 1), QPointF(1, 1)));
}

CubicBezierSpline::CubicBezierSpline(const CubicBezierSpline& spline, QObject* parent) :
        QObject(parent)
{
    m_precision = spline.m_precision;
    m_points = spline.m_points;
    m_validSpline = false;
}

CubicBezierSpline& CubicBezierSpline::operator=(const CubicBezierSpline& spline)
{
    m_precision = spline.m_precision;
    m_points = spline.m_points;
    m_validSpline = false;
    return *this;
}

void CubicBezierSpline::fromString(const QString& spline)
{
    m_points.clear();
    m_validSpline = false;

    QStringList bpoints = spline.split('|');
    foreach(const QString &bpoint, bpoints) {
        QStringList points = bpoint.split('#');
        QList <QPointF> values;
        foreach(const QString &point, points) {
            QStringList xy = point.split(';');
            if (xy.count() == 2)
                values.append(QPointF(xy.at(0).toDouble(), xy.at(1).toDouble()));
        }
        if (values.count() == 3) {
            m_points.append(BPoint(values.at(0), values.at(1), values.at(2)));
        }
    }

    keepSorted();
    validatePoints();
}

QString CubicBezierSpline::toString() const
{
    QStringList spline;
    foreach(const BPoint &p, m_points) {
        spline << QString("%1;%2#%3;%4#%5;%6").arg(p.h1.x()).arg(p.h1.y())
                                              .arg(p.p.x()).arg(p.p.y())
                                              .arg(p.h2.x()).arg(p.h2.y());
    }
    return spline.join("|");
}

int CubicBezierSpline::setPoint(int ix, const BPoint& point)
{
    m_points[ix] = point;
    keepSorted();
    validatePoints();
    m_validSpline = false;
    return indexOf(point); // in case it changed
}

QList <BPoint> CubicBezierSpline::points()
{
    return m_points;
}

void CubicBezierSpline::removePoint(int ix)
{
    m_points.removeAt(ix);
    m_validSpline = false;
}

int CubicBezierSpline::addPoint(const BPoint& point)
{
    m_points.append(point);
    keepSorted();
    validatePoints();
    m_validSpline = false;
    return indexOf(point);
}

void CubicBezierSpline::setPrecision(int pre)
{
    if (pre != m_precision) {
        m_precision = pre;
        m_validSpline = false;
    }
}

int CubicBezierSpline::getPrecision()
{
    return m_precision;
}

qreal CubicBezierSpline::value(qreal x, bool cont)
{
    update();

    if (!cont)
        m_i = m_spline.constBegin();
    if (m_i != m_spline.constBegin())
        --m_i;

    double diff = qAbs(x - m_i.key());
    double y = m_i.value();
    while (m_i != m_spline.constEnd()) {
        if (qAbs(x - m_i.key()) > diff)
            break;

        diff = qAbs(x - m_i.key());
        y = m_i.value();
        ++m_i;
    }
    return qBound((qreal)0.0, y, (qreal)1.0);
}

void CubicBezierSpline::validatePoints()
{
    BPoint p1, p2;
    for (int i = 0; i < m_points.count() - 1; ++i) {
        p1 = m_points.at(i);
        p2 = m_points.at(i+1);
        p1.h2.setX(qBound(p1.p.x(), p1.h2.x(), p2.p.x()));
        p2.h1.setX(qBound(p1.p.x(), p2.h1.x(), p2.p.x()));
        m_points[i] = p1;
        m_points[i+1] = p2;
    }
}

void CubicBezierSpline::keepSorted()
{
    qSort(m_points.begin(), m_points.end(), pointLessThan);
}

QPointF CubicBezierSpline::point(double t, const QList< QPointF >& points)
{
    /*
     * Calculating a point on the bezier curve using the coefficients from Bernstein basis polynomial of degree 3.
     * Using the De Casteljau algorithm would be slightly faster for when calculating a lot of values
     * but the difference is far from noticable in this needcase
     */
    double c1 = (1-t) * (1-t) * (1-t);
    double c2 = 3 * t * (1-t) * (1-t);
    double c3 = 3 * t * t * (1-t);
    double c4 = t * t * t;
    
    return QPointF(points[0].x()*c1 + points[1].x()*c2 + points[2].x()*c3 + points[3].x()*c4,
                   points[0].y()*c1 + points[1].y()*c2 + points[2].y()*c3 + points[3].y()*c4);
}

void CubicBezierSpline::update()
{
    if (m_validSpline)
        return;

    m_validSpline = true;
    m_spline.clear();

    QList <QPointF> points;
    QPointF p;
    for (int i = 0; i < m_points.count() - 1; ++i) {
        points.clear();
        points << m_points.at(i).p
                << m_points.at(i).h2
                << m_points.at(i+1).h1
                << m_points.at(i+1).p;

        int numberOfValues = (int)((points[3].x() - points[0].x()) * m_precision * 10);
        if (numberOfValues == 0)
            numberOfValues = 1;
        double step = 1 / (double)numberOfValues;
        double t = 0;
        while (t <= 1) {
            p = point(t, points);
            m_spline.insert(p.x(), p.y());
            t += step;
        }
    }
}

int CubicBezierSpline::indexOf(const BPoint& p)
{
    if (m_points.indexOf(p) == -1) {
        // point changed during validation process
        for (int i = 0; i < m_points.count(); ++i) {
            // this condition should be sufficient, too
            if (m_points.at(i).p == p.p)
                return i;
        }
    } else {
        return m_points.indexOf(p);
    }
    return -1;
}

#include "cubicbezierspline.moc"