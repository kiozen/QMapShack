/**********************************************************************************************
    Copyright (C) 2014 Oliver Eichler <oliver.eichler@gmx.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************************/

#include "canvas/CCanvas.h"
#include "CMainWindow.h"
#include "gis/Poi.h"
#include "GeoMath.h"
#include "helpers/CDraw.h"
#include "helpers/CProgressDialog.h"
#include "map/CMapDraw.h"
#include "map/CMapException.h"
#include "map/CMapIMG.h"
#include "map/garmin/CGarminSubfile.h"
#include "map/garmin/CGarminTyp.h"
#include "units/IUnit.h"

#include <QPainterPath>
#include <QtWidgets>

#undef DEBUG_SHOW_SECT_DESC
#undef DEBUG_SHOW_TRE_DATA
#undef DEBUG_SHOW_SUBDIV_DATA
#undef DEBUG_SHOW_MAPLEVELS
#undef DEBUG_SHOW_SECTION_BORDERS
#undef DEBUG_SHOW_SUBDIV_BORDERS

#define STREETNAME_THRESHOLD 5.0

int CFileExt::cnt = 0;

static inline bool isCompletelyOutside(const QPolygonF& poly, const QRectF &viewport)
{
    qreal north =  -90.0 * DEG_TO_RAD;
    qreal south =   90.0 * DEG_TO_RAD;
    qreal west  =  180.0 * DEG_TO_RAD;
    qreal east  = -180.0 * DEG_TO_RAD;

    for(const QPointF &pt : poly)
    {
        if(north < pt.y())
        {
            north = pt.y();
        }
        if(south > pt.y())
        {
            south = pt.y();
        }
        if(west  > pt.x())
        {
            west  = pt.x();
        }
        if(east  < pt.x())
        {
            east  = pt.x();
        }
    }

    QRectF ref(west, north,  east - west, south - north);

    if(ref.width() == 0)
    {
        ref.setWidth(0.00001);
    }
    if(ref.height() == 0)
    {
        ref.setHeight(0.00001);
    }

    return !viewport.intersects(ref);
}

static inline QImage img2line(const QImage &img, int width)
{
    Q_ASSERT(img.format() == QImage::Format_ARGB32_Premultiplied);

    QImage newImage(width, img.height(), QImage::Format_ARGB32_Premultiplied);

    const int bpl_src = img.bytesPerLine();
    const int bpl_dst = newImage.bytesPerLine();
    const uchar *_srcBits = img.bits();
    uchar *_dstBits = newImage.bits();

    for(int i = 0; i < img.height(); i++)
    {
        const uchar *srcBits = _srcBits + bpl_src * i;
        uchar *dstBits = _dstBits + bpl_dst * i;

        int bytesToCopy = bpl_dst;
        while(bytesToCopy > 0)
        {
            memcpy(dstBits, srcBits, qMin(bytesToCopy, bpl_src));
            dstBits += bpl_src;
            bytesToCopy -= bpl_src;
        }
    }
    return newImage;
}

static inline bool isCluttered(QVector<QRectF>& rectPois, const QRectF& rect)
{
    for(const QRectF &rectPoi : rectPois)
    {
        if(rect.intersects(rectPoi))
        {
            return true;
        }
    }
    rectPois << rect;
    return false;
}


CMapIMG::CMapIMG(const QString &filename, CMapDraw *parent)
    : IMap(eFeatVisibility | eFeatVectorItems | eFeatTypFile, parent)
    , filename(filename)
    , fm(CMainWindow::self().getMapFont())
    , selectedLanguage(NOIDX)
{
    qDebug() << "------------------------------";
    qDebug() << "IMG: try to open" << filename;

    try
    {
        readBasics();
        processPrimaryMapData();
        setupTyp();
    }
    catch(const CMapException& e)
    {
        QMessageBox::critical(CMainWindow::getBestWidgetForParent(), tr("Failed ..."), e.msg, QMessageBox::Abort);
        return;
    }


    isActivated = true;
}

void CMapIMG::loadConfig(QSettings& cfg)
{
    IMap::loadConfig(cfg);

    if(!typeFile.isEmpty())
    {
        setupTyp();
    }
}

void CMapIMG::slotSetTypeFile(const QString& filename)
{
    IMap::slotSetTypeFile(filename);
    setupTyp();
    CCanvas::triggerCompleteUpdate(CCanvas::eRedrawMap);
}


void CMapIMG::setupTyp()
{
    languages.clear();
    languages[0x00] = tr("Unspecified");
    languages[0x01] = tr("French");
    languages[0x02] = tr("German");
    languages[0x03] = tr("Dutch");
    languages[0x04] = tr("English");
    languages[0x05] = tr("Italian");
    languages[0x06] = tr("Finnish");
    languages[0x07] = tr("Swedish");
    languages[0x08] = tr("Spanish");
    languages[0x09] = tr("Basque");
    languages[0x0a] = tr("Catalan");
    languages[0x0b] = tr("Galician");
    languages[0x0c] = tr("Welsh");
    languages[0x0d] = tr("Gaelic");
    languages[0x0e] = tr("Danish");
    languages[0x0f] = tr("Norwegian");
    languages[0x10] = tr("Portuguese");
    languages[0x11] = tr("Slovak");
    languages[0x12] = tr("Czech");
    languages[0x13] = tr("Croatian");
    languages[0x14] = tr("Hungarian");
    languages[0x15] = tr("Polish");
    languages[0x16] = tr("Turkish");
    languages[0x17] = tr("Greek");
    languages[0x18] = tr("Slovenian");
    languages[0x19] = tr("Russian");
    languages[0x1a] = tr("Estonian");
    languages[0x1b] = tr("Latvian");
    languages[0x1c] = tr("Romanian");
    languages[0x1d] = tr("Albanian");
    languages[0x1e] = tr("Bosnian");
    languages[0x1f] = tr("Lithuanian");
    languages[0x20] = tr("Serbian");
    languages[0x21] = tr("Macedonian");
    languages[0x22] = tr("Bulgarian");

    polylineProperties.clear();
    polylineProperties[0x01] = CGarminTyp::polyline_property(0x01, Qt::blue,   6, Qt::SolidLine );
    polylineProperties[0x01].penBorderDay = QPen(Qt::black, 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x01].penBorderNight = QPen(Qt::lightGray, 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x01].hasBorder = true;
    polylineProperties[0x02] = CGarminTyp::polyline_property(0x02, "#cc9900",   4, Qt::SolidLine );
    polylineProperties[0x02].penBorderDay = QPen(Qt::black, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x02].penBorderNight = QPen(Qt::lightGray, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x02].hasBorder = true;
    polylineProperties[0x03] = CGarminTyp::polyline_property(0x03, Qt::yellow,   3, Qt::SolidLine );
    polylineProperties[0x03].penBorderDay = QPen(Qt::black, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x03].penBorderNight = QPen(Qt::lightGray, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x03].hasBorder = true;
    polylineProperties[0x04] = CGarminTyp::polyline_property(0x04, "#ffff00",   3, Qt::SolidLine );
    polylineProperties[0x04].penBorderDay = QPen(Qt::black, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x04].penBorderNight = QPen(Qt::lightGray, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x04].hasBorder = true;
    polylineProperties[0x05] = CGarminTyp::polyline_property(0x05, "#dc7c5a",   2, Qt::SolidLine );
    polylineProperties[0x06] = CGarminTyp::polyline_property(0x06, Qt::gray,   2, Qt::SolidLine );
    polylineProperties[0x06].penBorderDay = QPen(Qt::black, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x06].penBorderNight = QPen(QColor("#f0f0f0"), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x06].hasBorder = true;
    polylineProperties[0x07] = CGarminTyp::polyline_property(0x07, "#c46442",   1, Qt::SolidLine );
    polylineProperties[0x08] = CGarminTyp::polyline_property(0x08, "#e88866",   2, Qt::SolidLine );
    polylineProperties[0x09] = CGarminTyp::polyline_property(0x09, "#e88866",   2, Qt::SolidLine );
    polylineProperties[0x0A] = CGarminTyp::polyline_property(0x0A, "#808080",   2, Qt::SolidLine );
    polylineProperties[0x0B] = CGarminTyp::polyline_property(0x0B, "#c46442",   2, Qt::SolidLine );
    polylineProperties[0x0C] = CGarminTyp::polyline_property(0x0C, "#000000",   2, Qt::SolidLine );
    polylineProperties[0x14] = CGarminTyp::polyline_property(0x14, Qt::white,   2, Qt::DotLine   );
    polylineProperties[0x14].penBorderDay = QPen(Qt::black, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x14].penBorderNight = QPen(Qt::lightGray, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    polylineProperties[0x14].hasBorder = true;
    polylineProperties[0x15] = CGarminTyp::polyline_property(0x15, "#000080",   2, Qt::SolidLine );
    polylineProperties[0x16] = CGarminTyp::polyline_property(0x16, "#E0E0E0",   1, Qt::SolidLine );
    polylineProperties[0x18] = CGarminTyp::polyline_property(0x18, "#0000ff",   2, Qt::SolidLine );
    polylineProperties[0x19] = CGarminTyp::polyline_property(0x19, "#00ff00",   2, Qt::SolidLine );
    polylineProperties[0x1A] = CGarminTyp::polyline_property(0x1A, "#000000",   2, Qt::SolidLine );
    polylineProperties[0x1B] = CGarminTyp::polyline_property(0x1B, "#000000",   2, Qt::SolidLine );
    polylineProperties[0x1C] = CGarminTyp::polyline_property(0x1C, "#00c864",   2, Qt::DotLine   );
    polylineProperties[0x1D] = CGarminTyp::polyline_property(0x1D, "#00c864",   2, Qt::DotLine   );
    polylineProperties[0x1E] = CGarminTyp::polyline_property(0x1E, "#00c864",   2, Qt::DotLine   );
    polylineProperties[0x1F] = CGarminTyp::polyline_property(0x1F, "#0000ff",   2, Qt::SolidLine );
    polylineProperties[0x20] = CGarminTyp::polyline_property(0x20, "#b67824",   1, Qt::SolidLine );
    polylineProperties[0x21] = CGarminTyp::polyline_property(0x21, "#b67824",   2, Qt::SolidLine );
    polylineProperties[0x22] = CGarminTyp::polyline_property(0x22, "#b67824",   3, Qt::SolidLine );
    polylineProperties[0x23] = CGarminTyp::polyline_property(0x23, "#b67824",   1, Qt::SolidLine );
    polylineProperties[0x24] = CGarminTyp::polyline_property(0x24, "#b67824",   2, Qt::SolidLine );
    polylineProperties[0x25] = CGarminTyp::polyline_property(0x25, "#b67824",   3, Qt::SolidLine );
    polylineProperties[0x26] = CGarminTyp::polyline_property(0x26, "#0000ff",   2, Qt::DotLine   );
    polylineProperties[0x27] = CGarminTyp::polyline_property(0x27, "#c46442",   4, Qt::SolidLine );
    polylineProperties[0x28] = CGarminTyp::polyline_property(0x28, "#aa0000",   2, Qt::SolidLine );
    polylineProperties[0x29] = CGarminTyp::polyline_property(0x29, "#ff0000",   2, Qt::SolidLine );
    polylineProperties[0x2A] = CGarminTyp::polyline_property(0x2A, "#000000",   2, Qt::SolidLine );
    polylineProperties[0x2B] = CGarminTyp::polyline_property(0x2B, "#000000",   2, Qt::SolidLine );

    polylineProperties[0x01].strings[0x00] = tr("Major highway");
    polylineProperties[0x02].strings[0x00] = tr("Principal highway");
    polylineProperties[0x03].strings[0x00] = tr("Other highway");
    polylineProperties[0x04].strings[0x00] = tr("Arterial road");
    polylineProperties[0x05].strings[0x00] = tr("Collector road");
    polylineProperties[0x06].strings[0x00] = tr("Residential street");
    polylineProperties[0x07].strings[0x00] = tr("Alley/Private road");
    polylineProperties[0x08].strings[0x00] = tr("Highway ramp, low speed");
    polylineProperties[0x09].strings[0x00] = tr("Highway ramp, high speed");
    polylineProperties[0x0a].strings[0x00] = tr("Unpaved road");
    polylineProperties[0x0b].strings[0x00] = tr("Major highway connector");
    polylineProperties[0x0c].strings[0x00] = tr("Roundabout");
    polylineProperties[0x14].strings[0x00] = tr("Railroad");
    polylineProperties[0x15].strings[0x00] = tr("Shoreline");
    polylineProperties[0x16].strings[0x00] = tr("Trail");
    polylineProperties[0x18].strings[0x00] = tr("Stream");
    polylineProperties[0x19].strings[0x00] = tr("Timezone");
    polylineProperties[0x1a].strings[0x00] = tr("Ferry");
    polylineProperties[0x1b].strings[0x00] = tr("Ferry");
    polylineProperties[0x1c].strings[0x00] = tr("State/province border");
    polylineProperties[0x1d].strings[0x00] = tr("County/parish border");
    polylineProperties[0x1e].strings[0x00] = tr("International border");
    polylineProperties[0x1f].strings[0x00] = tr("River");
    polylineProperties[0x20].strings[0x00] = tr("Minor land contour");
    polylineProperties[0x21].strings[0x00] = tr("Intermediate land contour");
    polylineProperties[0x22].strings[0x00] = tr("Major land contour");
    polylineProperties[0x23].strings[0x00] = tr("Minor depth contour");
    polylineProperties[0x24].strings[0x00] = tr("Intermediate depth contour");
    polylineProperties[0x25].strings[0x00] = tr("Major depth contour");
    polylineProperties[0x26].strings[0x00] = tr("Intermittent stream");
    polylineProperties[0x27].strings[0x00] = tr("Airport runway");
    polylineProperties[0x28].strings[0x00] = tr("Pipeline");
    polylineProperties[0x29].strings[0x00] = tr("Powerline");
    polylineProperties[0x2a].strings[0x00] = tr("Marine boundary");
    polylineProperties[0x2b].strings[0x00] = tr("Hazard boundary");

    polygonProperties.clear();
    polygonProperties[0x01] = CGarminTyp::polygon_property(0x01, Qt::NoPen, "#d2c0c0", Qt::SolidPattern);
    polygonProperties[0x02] = CGarminTyp::polygon_property(0x02, Qt::NoPen, "#fbeab7", Qt::SolidPattern);
    polygonProperties[0x03] = CGarminTyp::polygon_property(0x03, Qt::NoPen, "#a4b094", Qt::SolidPattern);
    polygonProperties[0x04] = CGarminTyp::polygon_property(0x04, Qt::NoPen, "#808080", Qt::SolidPattern);
    polygonProperties[0x05] = CGarminTyp::polygon_property(0x05, Qt::NoPen, "#f0f0f0", Qt::SolidPattern);
    polygonProperties[0x06] = CGarminTyp::polygon_property(0x06, Qt::NoPen, "#cacaca", Qt::SolidPattern);
    polygonProperties[0x07] = CGarminTyp::polygon_property(0x07, Qt::NoPen, "#feebcf", Qt::SolidPattern);
    polygonProperties[0x08] = CGarminTyp::polygon_property(0x08, Qt::NoPen, "#fde8d5", Qt::SolidPattern);
    polygonProperties[0x09] = CGarminTyp::polygon_property(0x09, Qt::NoPen, "#fee8b8", Qt::SolidPattern);
    polygonProperties[0x0a] = CGarminTyp::polygon_property(0x0a, Qt::NoPen, "#fdeac6", Qt::SolidPattern);
    polygonProperties[0x0b] = CGarminTyp::polygon_property(0x0b, Qt::NoPen, "#fddfbd", Qt::SolidPattern);
    polygonProperties[0x0c] = CGarminTyp::polygon_property(0x0c, Qt::NoPen, "#ebeada", Qt::SolidPattern);
    polygonProperties[0x0d] = CGarminTyp::polygon_property(0x0d, Qt::NoPen, "#f8e3be", Qt::SolidPattern);
    polygonProperties[0x0e] = CGarminTyp::polygon_property(0x0e, Qt::NoPen, "#e0e0e0", Qt::SolidPattern);
    polygonProperties[0x13] = CGarminTyp::polygon_property(0x13, Qt::NoPen, "#cc9900", Qt::SolidPattern);
    polygonProperties[0x14] = CGarminTyp::polygon_property(0x14, Qt::NoPen, "#b7e999", Qt::SolidPattern);
    polygonProperties[0x15] = CGarminTyp::polygon_property(0x15, Qt::NoPen, "#b7e999", Qt::SolidPattern);
    polygonProperties[0x16] = CGarminTyp::polygon_property(0x16, Qt::NoPen, "#b7e999", Qt::SolidPattern);
    polygonProperties[0x17] = CGarminTyp::polygon_property(0x17, Qt::NoPen, "#90be00", Qt::SolidPattern);
    polygonProperties[0x18] = CGarminTyp::polygon_property(0x18, Qt::NoPen, "#00ff00", Qt::SolidPattern);
    polygonProperties[0x19] = CGarminTyp::polygon_property(0x19, Qt::NoPen, "#f8e3be", Qt::SolidPattern);
    polygonProperties[0x1a] = CGarminTyp::polygon_property(0x1a, Qt::NoPen, "#d3f5a5", Qt::SolidPattern);
    polygonProperties[0x1e] = CGarminTyp::polygon_property(0x1e, Qt::NoPen, "#b7e999", Qt::SolidPattern);
    polygonProperties[0x1f] = CGarminTyp::polygon_property(0x1f, Qt::NoPen, "#b7e999", Qt::SolidPattern);
    polygonProperties[0x20] = CGarminTyp::polygon_property(0x20, Qt::NoPen, "#b7e999", Qt::SolidPattern);
    polygonProperties[0x28] = CGarminTyp::polygon_property(0x28, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x29] = CGarminTyp::polygon_property(0x29, Qt::NoPen, "#0000ff", Qt::SolidPattern);
    polygonProperties[0x32] = CGarminTyp::polygon_property(0x32, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x3b] = CGarminTyp::polygon_property(0x3b, Qt::NoPen, "#0000ff", Qt::SolidPattern);
    polygonProperties[0x3c] = CGarminTyp::polygon_property(0x3c, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x3d] = CGarminTyp::polygon_property(0x3d, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x3e] = CGarminTyp::polygon_property(0x3e, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x3f] = CGarminTyp::polygon_property(0x3f, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x40] = CGarminTyp::polygon_property(0x40, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x41] = CGarminTyp::polygon_property(0x41, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x42] = CGarminTyp::polygon_property(0x42, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x43] = CGarminTyp::polygon_property(0x43, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x44] = CGarminTyp::polygon_property(0x44, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x45] = CGarminTyp::polygon_property(0x45, Qt::NoPen, "#0000ff", Qt::SolidPattern);
    polygonProperties[0x46] = CGarminTyp::polygon_property(0x46, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x47] = CGarminTyp::polygon_property(0x47, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x48] = CGarminTyp::polygon_property(0x48, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x49] = CGarminTyp::polygon_property(0x49, Qt::NoPen, "#0080ff", Qt::SolidPattern);
#ifdef WIN32
    polygonProperties[0x4a] = CGarminTyp::polygon_property(0x4a, "#000000", qRgba(255, 255, 255, 0), Qt::SolidPattern);
    polygonProperties[0x4b] = CGarminTyp::polygon_property(0x4b, "#000000", qRgba(255, 255, 255, 0), Qt::SolidPattern);
#else
    polygonProperties[0x4a] = CGarminTyp::polygon_property(0x4a, "#000000", Qt::transparent, Qt::NoBrush);
    polygonProperties[0x4b] = CGarminTyp::polygon_property(0x4b, "#000000", Qt::transparent, Qt::NoBrush);
#endif
    polygonProperties[0x4c] = CGarminTyp::polygon_property(0x4c, Qt::NoPen, "#f0e68c", Qt::SolidPattern);
    polygonProperties[0x4d] = CGarminTyp::polygon_property(0x4d, Qt::NoPen, "#00ffff", Qt::SolidPattern);
    polygonProperties[0x4e] = CGarminTyp::polygon_property(0x4e, Qt::NoPen, "#d3f5a5", Qt::SolidPattern);
    polygonProperties[0x4f] = CGarminTyp::polygon_property(0x4f, Qt::NoPen, "#d3f5a5", Qt::SolidPattern);
    polygonProperties[0x50] = CGarminTyp::polygon_property(0x50, Qt::NoPen, "#b7e999", Qt::SolidPattern);
    polygonProperties[0x51] = CGarminTyp::polygon_property(0x51, Qt::NoPen, "#0000ff", Qt::DiagCrossPattern);
    polygonProperties[0x52] = CGarminTyp::polygon_property(0x52, Qt::NoPen, "#4aca4a", Qt::SolidPattern);
    polygonProperties[0x53] = CGarminTyp::polygon_property(0x53, Qt::NoPen, "#bcedfa", Qt::SolidPattern);
    polygonProperties[0x54] = CGarminTyp::polygon_property(0x54, Qt::NoPen, "#fde8d5", Qt::SolidPattern);
    polygonProperties[0x59] = CGarminTyp::polygon_property(0x59, Qt::NoPen, "#0080ff", Qt::SolidPattern);
    polygonProperties[0x69] = CGarminTyp::polygon_property(0x69, Qt::NoPen, "#0080ff", Qt::SolidPattern);

    polygonProperties[0x01].strings[0x00] = tr("Large urban area (&gt;200K)");
    polygonProperties[0x02].strings[0x00] = tr("Small urban area (&lt;200K)");
    polygonProperties[0x03].strings[0x00] = tr("Rural housing area");
    polygonProperties[0x04].strings[0x00] = tr("Military base");
    polygonProperties[0x05].strings[0x00] = tr("Parking lot");
    polygonProperties[0x06].strings[0x00] = tr("Parking garage");
    polygonProperties[0x07].strings[0x00] = tr("Airport");
    polygonProperties[0x08].strings[0x00] = tr("Shopping center");
    polygonProperties[0x09].strings[0x00] = tr("Marina");
    polygonProperties[0x0a].strings[0x00] = tr("University/College");
    polygonProperties[0x0b].strings[0x00] = tr("Hospital");
    polygonProperties[0x0c].strings[0x00] = tr("Industrial complex");
    polygonProperties[0x0d].strings[0x00] = tr("Reservation");
    polygonProperties[0x0e].strings[0x00] = tr("Airport runway");
    polygonProperties[0x13].strings[0x00] = tr("Man-made area");
    polygonProperties[0x19].strings[0x00] = tr("Sports complex");
    polygonProperties[0x18].strings[0x00] = tr("Golf course");
    polygonProperties[0x1a].strings[0x00] = tr("Cemetery");
    polygonProperties[0x14].strings[0x00] = tr("National park");
    polygonProperties[0x15].strings[0x00] = tr("National park");
    polygonProperties[0x16].strings[0x00] = tr("National park");
    polygonProperties[0x17].strings[0x00] = tr("City park");
    polygonProperties[0x1e].strings[0x00] = tr("State park");
    polygonProperties[0x1f].strings[0x00] = tr("State park");
    polygonProperties[0x20].strings[0x00] = tr("State park");
    polygonProperties[0x50].strings[0x00] = tr("Forest");
    polygonProperties[0x28].strings[0x00] = tr("Ocean");
    polygonProperties[0x29].strings[0x00] = tr("Blue (unknown)");
    polygonProperties[0x32].strings[0x00] = tr("Sea");
    polygonProperties[0x3b].strings[0x00] = tr("Blue (unknown)");
    polygonProperties[0x3c].strings[0x00] = tr("Large lake");
    polygonProperties[0x3d].strings[0x00] = tr("Large lake");
    polygonProperties[0x3e].strings[0x00] = tr("Medium lake");
    polygonProperties[0x3f].strings[0x00] = tr("Medium lake");
    polygonProperties[0x40].strings[0x00] = tr("Small lake");
    polygonProperties[0x41].strings[0x00] = tr("Small lake");
    polygonProperties[0x42].strings[0x00] = tr("Major lake");
    polygonProperties[0x43].strings[0x00] = tr("Major lake");
    polygonProperties[0x44].strings[0x00] = tr("Large lake");
    polygonProperties[0x46].strings[0x00] = tr("Blue (unknown)");
    polygonProperties[0x46].strings[0x00] = tr("Major River");
    polygonProperties[0x47].strings[0x00] = tr("Large River");
    polygonProperties[0x48].strings[0x00] = tr("Medium River");
    polygonProperties[0x49].strings[0x00] = tr("Small River");
    //    polygonProperties[0x4a].strings[0x00] = tr("Definition area");
    //    polygonProperties[0x4b].strings[0x00] = tr("Background");
    polygonProperties[0x4c].strings[0x00] = tr("Intermittent water");
    polygonProperties[0x51].strings[0x00] = tr("Wetland/Swamp");
    polygonProperties[0x4d].strings[0x00] = tr("Glacier");
    polygonProperties[0x4e].strings[0x00] = tr("Orchard/Plantation");
    polygonProperties[0x4f].strings[0x00] = tr("Scrub");
    polygonProperties[0x52].strings[0x00] = tr("Tundra");
    polygonProperties[0x53].strings[0x00] = tr("Flat");
    polygonProperties[0x54].strings[0x00] = tr("???");
    polygonDrawOrder.clear();
    for(int i = 0; i < 0x80; i++)
    {
        polygonDrawOrder << (0x7F - i);
    }

    pointProperties.clear();

    if(!typeFile.isEmpty())
    {
        QFile file(typeFile);
        if(!file.open(QIODevice::ReadOnly))
        {
            QMessageBox::warning(CMainWindow::self().getBestWidgetForParent(), tr("Read external type file..."), tr("Failed to read type file: %1\nFall back to internal types.").arg(typeFile), QMessageBox::Ok);
            typeFile.clear();
            setupTyp();
            return;
        }

        QByteArray array = file.readAll();
        CGarminTyp typ;
        typ.decode(array, polygonProperties, polylineProperties, polygonDrawOrder, pointProperties);

        file.close();
    }
    else
    {
        for(const CGarminSubfile& subfile : subfiles)
        {
            const CGarminSubfile::part_t& part = subfile.getPart("TYP");
            if(part.offsetHead == CGarminSubfile::NO_OFFSET)
            {
                continue;
            }

            CFileExt file(filename);
            file.open(QIODevice::ReadOnly);

            QByteArray array;
            readFile(file, part.offsetHead, part.size, array);

            CGarminTyp typ;
            typ.decode(array, polygonProperties, polylineProperties, polygonDrawOrder, pointProperties);

            file.close();
            break;
        }
    }
}

void CMapIMG::readFile(CFileExt& file, quint32 offset, quint32 size, QByteArray& data)
{
    if(offset + size > file.size())
    {
        throw CMapException(CMapException::eErrOpen, tr("Failed to read: ") + file.fileName());
    }

    data = QByteArray::fromRawData(file.data(offset, size), size);
}


void CMapIMG::readBasics()
{
    char tmpstr[64];
    qint64 fsize    = QFileInfo(filename).size();

    CFileExt file(filename);
    if(!file.open(QIODevice::ReadOnly))
    {
        throw CMapException(CMapException::eErrOpen, tr("Failed to open: ") + filename);
    }

    quint8 mask = (quint8) * file.data(0, 1);

    if(mask != 0)
    {
        throw CMapException(CMapException::eErrOpen, tr("File contains locked / encrypted data. Garmin does not "
                                                        "want you to use this file with any other software than "
                                                        "the one supplied by Garmin."));
    }

    // read hdr_img_t
    QByteArray imghdr;
    readFile(file, 0, sizeof(hdr_img_t), imghdr);
    hdr_img_t * pImgHdr = (hdr_img_t*)imghdr.data();

    if(strncmp(pImgHdr->signature, "DSKIMG", 7) != 0)
    {
        throw CMapException(CMapException::errFormat, tr("Bad file format: ") + filename);
    }
    if(strncmp(pImgHdr->identifier, "GARMIN", 7) != 0)
    {
        throw CMapException(CMapException::errFormat, tr("Bad file format: ") + filename);
    }

    mapdesc  = QByteArray((const char*)pImgHdr->desc1, 20);
    mapdesc += pImgHdr->desc2;
    qDebug() << mapdesc;

    size_t blocksize = pImgHdr->blocksize();

    size_t offsetFAT = gar_load(quint8, pImgHdr->offsetFAT) * 0x200;
    size_t dataoffset = offsetFAT;

    // 1st read FAT
    QByteArray FATblock;
    readFile(file, dataoffset, sizeof(FATblock_t), FATblock);
    const FATblock_t * pFATBlock = (const FATblock_t * )FATblock.data();

    // start of new subfile part
    /*
        It is taken for granted that the single subfile parts are not
        fragmented within the file. Thus it is not really necessary to
        store and handle all block sequence numbers. Just the first one
        will give us the offset. This also implies that it is not necessary
        to care about FAT blocks with a non-zero part number.

        2007-03-31: Garmin's world base map seems to be coded different.
                    The part field seems to be rather a bit field than
                    a part number. As the total subfile size is given
                    for the first part only (for all others it's zero)
                    I use it to identify the 1st part of a subfile

        2007-05-26: Gmapsupp images by Sendmap code quite some bull shit,
                    too. The size is stored in every part and they do have
                    a part number. I introduced a set of subfile names
                    storing the subfile's name and type. The first part
                    with a size info and it's name / type not stored in the
                    set is used to get the location information.
     */
    QSet<QString> subfileNames;
    while(dataoffset < (size_t)fsize)
    {
        if(pFATBlock->flag != 0x01)
        {
            break;
        }

        memcpy(tmpstr, pFATBlock->name, sizeof(pFATBlock->name) + sizeof(pFATBlock->type));
        tmpstr[sizeof(pFATBlock->name) + sizeof(pFATBlock->type)] = 0;

        if(gar_load(quint32, pFATBlock->size) != 0 && !subfileNames.contains(tmpstr) && tmpstr[0] != 0x20)
        {
            subfileNames << tmpstr;

            memcpy(tmpstr, pFATBlock->name, sizeof(pFATBlock->name));
            tmpstr[sizeof(pFATBlock->name)] = 0;

            // skip MAPSORC.MPS section
            if(strcmp(tmpstr, "MAPSOURC") && strcmp(tmpstr, "SENDMAP2"))
            {
                CGarminSubfile& subfile = subfiles[tmpstr];

                memcpy(tmpstr, pFATBlock->type, sizeof(pFATBlock->type));
                tmpstr[sizeof(pFATBlock->type)] = 0;

                subfile.setPart(
                    tmpstr,
                    quint32(gar_load(uint16_t, pFATBlock->blocks[0]) * blocksize),
                    gar_load(quint32, pFATBlock->size)
                    );
            }
        }

        dataoffset += sizeof(FATblock_t);
        readFile(file, quint32(dataoffset), quint32(sizeof(FATblock_t)), FATblock);
        pFATBlock = (const FATblock_t * )FATblock.data();
    }

    if((dataoffset == offsetFAT) || (dataoffset >= (size_t)fsize))
    {
        throw CMapException(CMapException::errFormat, tr("Failed to read file structure: ") + filename);
    }

#ifdef DEBUG_SHOW_SECT_DESC
    {
        QMap<QString, subfile_desc_t>::const_iterator subfile = subfiles.begin();
        while(subfile != subfiles.end())
        {
            qDebug() << "--- subfile" << subfile->name << "---";
            QMap<QString, subfile_part_t>::const_iterator part = subfile->parts.begin();
            while(part != subfile->parts.end())
            {
                qDebug() << part.key() << hex << part->offset << part->size;
                ++part;
            }
            ++subfile;
        }
    }
#endif                       //DEBUG_SHOW_SECT_DESC

    int cnt = 1;
    int tot = subfiles.count();

    PROGRESS_SETUP(tr("Loading %1").arg(QFileInfo(filename).fileName()), 0, tot, CMainWindow::getBestWidgetForParent());

    maparea = QRectF();
    for(CGarminSubfile& subfile : subfiles)
    {
        PROGRESS(cnt++, throw CMapException(CMapException::errAbort, tr("User abort: ") + filename));
        if(subfile.isGMP())
        {
            throw CMapException(CMapException::errFormat, tr("File is NT format. QMapShack is unable to read map files with NT format: ") + filename);
        }

        readSubfileBasics(subfile, file);
    }

    // combine copyright sections
    copyright.clear();
    for(const QString &str : copyrights)
    {
        if(!copyright.isEmpty())
        {
            copyright += "\n";
        }
        copyright += str;
    }

    qDebug() << "dimensions:\t" << "N" << (maparea.bottom() * RAD_TO_DEG) << "E" << (maparea.right() * RAD_TO_DEG) << "S" << (maparea.top() * RAD_TO_DEG) << "W" << (maparea.left() * RAD_TO_DEG);
}

void CMapIMG::readSubfileBasics(CGarminSubfile &subfile, CFileExt &file)
{
    subfile.readBasics(file);
    copyrights << subfile.getCopyright();

    if(maparea.isNull())
    {
        maparea = subfile.getArea();
    }
    else
    {
        maparea = maparea.united(subfile.getArea());
    }
}

void CMapIMG::processPrimaryMapData()
{
    /*
     * Query all subfiles for possible maplevels.
     * Exclude basemap to avoid pollution.
     */
    for(const CGarminSubfile &subfile : subfiles)
    {
        for(const CGarminSubfile::map_level_t &maplevel : subfile.getMaplevels())
        {
            if(!maplevel.inherited)
            {
                CGarminSubfile::map_level_t ml;
                ml.bits  = maplevel.bits;
                ml.level = maplevel.level;
                ml.inherited = false;
                maplevels << ml;
            }
        }
    }

    /* Sort all entries, note that stable sort should insure that basemap is preferred when available. */
    qStableSort(maplevels.begin(), maplevels.end(), CGarminSubfile::map_level_t::GreaterThan);
    /* Delete any duplicates for obvious performance reasons. */
    auto where = std::unique(maplevels.begin(), maplevels.end());
    maplevels.erase(where, maplevels.end());


#ifdef DEBUG_SHOW_MAPLEVELS
    for(int i = 0; i < maplevels.count(); ++i)
    {
        map_level_t& ml = maplevels[i];
        qDebug() << ml.bits << ml.level << ml.useBaseMap;
    }
#endif
}


quint8 CMapIMG::scale2bits(const QPointF& scale)
{
    qint32 bits = 24;
    if(scale.x() >= 70000.0)
    {
        bits = 2;
    }
    else if(scale.x() >= 50000.0)
    {
        bits = 3;
    }
    else if(scale.x() >= 30000.0)
    {
        bits = 4;
    }
    else if(scale.x() >= 20000.0)
    {
        bits = 5;
    }
    else if(scale.x() >= 15000.0)
    {
        bits = 6;
    }
    else if(scale.x() >= 10000.0)
    {
        bits = 7;
    }
    else if(scale.x() >= 7000.0)
    {
        bits = 8;
    }
    else if(scale.x() >= 5000.0)
    {
        bits = 9;
    }
    else if(scale.x() >= 3000.0)
    {
        bits = 10;
    }
    else if(scale.x() >= 2000.0)
    {
        bits = 11;
    }
    else if(scale.x() >= 1500.0)
    {
        bits = 12;
    }
    else if(scale.x() >= 1000.0)
    {
        bits = 13;
    }
    else if(scale.x() >= 700.0)
    {
        bits = 14;
    }
    else if(scale.x() >= 500.0)
    {
        bits = 15;
    }
    else if(scale.x() >= 300.0)
    {
        bits = 16;
    }
    else if(scale.x() >= 200.0)
    {
        bits = 17;
    }
    else if(scale.x() >= 100.0)
    {
        bits = 18;
    }
    else if(scale.x() >= 70.0)
    {
        bits = 19;
    }
    else if(scale.x() >= 30.0)
    {
        bits = 20;
    }
    else if(scale.x() >= 15.0)
    {
        bits = 21;
    }
    else if(scale.x() >= 7.0)
    {
        bits = 22;
    }
    else if(scale.x() >= 3.0)
    {
        bits = 23;
    }

    return qMax(2, qMin(24, bits + getAdjustDetailLevel()));
}

void CMapIMG::draw(IDrawContext::buffer_t& buf) /* override */
{
    if(map->needsRedraw())
    {
        return;
    }

    QPointF bufferScale = buf.scale * buf.zoomFactor;

    if(isOutOfScale(bufferScale))
    {
        return;
    }

    QPainter p(&buf.image);
    p.setOpacity(getOpacity() / 100.0);
    USE_ANTI_ALIASING(p, true);

    QFont f = CMainWindow::self().getMapFont();
    fm = QFontMetrics(f);

    p.setFont(f);
    p.setPen(Qt::black);
    p.setBrush(Qt::NoBrush);

    quint8 bits = scale2bits(bufferScale);

    QVector<CGarminSubfile::map_level_t>::const_iterator maplevel = maplevels.constEnd();
    do
    {
        --maplevel;
        if(bits >= maplevel->bits)
        {
            break;
        }
    }
    while(maplevel != maplevels.constBegin());

    qreal u1 = qMin(buf.ref1.x(), buf.ref4.x());
    qreal u2 = qMax(buf.ref2.x(), buf.ref3.x());
    qreal v1 = qMax(buf.ref1.y(), buf.ref2.y());
    qreal v2 = qMin(buf.ref4.y(), buf.ref3.y());

    QRectF viewport(u1, v1, u2 - u1, v2 - v1);
    QVector<QRectF> rectPois;

    polygons.clear();
    polylines.clear();
    pois.clear();
    points.clear();
    labels.clear();

    /**
       convertRad2Px() converts positions into screen coordinates. However the painter
       devices paints into the buffer which is a little bit larger than the screen.
       Thus we need the offset of the buffer's top left corner to the top left corner
       of the screen to adjust all drawings.
     */
    QPointF pp = buf.ref1;
    map->convertRad2Px(pp);
    p.save();
    p.translate(-pp);

    if(map->needsRedraw())
    {
        p.restore();
        return;
    }

    try
    {
        loadVisibleData(false, polygons, polylines, points, pois, maplevel->level, viewport, p);
    }
    catch(std::bad_alloc)
    {
        qWarning() << "GarminIMG: Allocation error. Abort map rendering.";
        p.restore();
        return;
    }

    if(map->needsRedraw())
    {
        p.restore();
        return;
    }
    drawPolygons(p, polygons);

    if(map->needsRedraw())
    {
        p.restore();
        return;
    }
    drawPolylines(p, polylines, bufferScale);

    if(map->needsRedraw())
    {
        p.restore();
        return;
    }
    drawPoints(p, points, rectPois);

    if(map->needsRedraw())
    {
        p.restore();
        return;
    }
    drawPois(p, pois, rectPois);

    if(map->needsRedraw())
    {
        p.restore();
        return;
    }
    drawText(p);

    if(map->needsRedraw())
    {
        p.restore();
        return;
    }
    drawLabels(p, labels);

    p.restore();
}

void CMapIMG::loadVisibleData(bool fast, polytype_t& polygons, polytype_t& polylines, pointtype_t& points, pointtype_t& pois, unsigned level, const QRectF& viewport, QPainter& p)
{
#ifndef Q_OS_WIN32
    CFileExt file(filename);
    if(!file.open(QIODevice::ReadOnly))
    {
        return;
    }
#endif

    for(const CGarminSubfile &subfile : subfiles)
    {
        if(!subfile.getArea().intersects(viewport))
        {
            continue;
        }

        if(map->needsRedraw())
        {
            break;
        }

#ifdef Q_OS_WIN32
        CFileExt file(filename);
        if(!file.open(QIODevice::ReadOnly))
        {
            return;
        }
#endif

        const QByteArray& rgndata = subfile.getRgnData(file);

        // collect polylines
        for(const CGarminSubfile::subdiv_desc_t &subdiv : subfile.getSubdivs())
        {
            // if(subdiv.level == level) qDebug() << "subdiv:" << subdiv.level << level <<  subdiv.area << viewport << subdiv.area.intersects(viewport);
            if(subdiv.level != level || !subdiv.area.intersects(viewport))
            {
                continue;
            }
            if(map->needsRedraw())
            {
                break;
            }
            loadSubDiv(file, subdiv, subfile.getStrtbl(), rgndata, fast, viewport, polylines, polygons, points, pois);

#ifdef DEBUG_SHOW_SECTION_BORDERS
            const QRectF& a = subdiv.area;
            qreal u[2] = {a.left(), a.right()};
            qreal v[2] = {a.top(), a.bottom()};

            QPolygonF poly;
            poly << a.bottomLeft() << a.bottomRight() << a.topRight() << a.topLeft();

            map->convertRad2Px(poly);

            p.setPen(QPen(Qt::magenta, 2));
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(poly);
#endif // DEBUG_SHOW_SECTION_BORDERS
        }

#ifdef DEBUG_SHOW_SUBDIV_BORDERS
        QPointF p1 = subfile.area.bottomLeft();
        QPointF p2 = subfile.area.bottomRight();
        QPointF p3 = subfile.area.topRight();
        QPointF p4 = subfile.area.topLeft();

        map->convertRad2Px(p1);
        map->convertRad2Px(p2);
        map->convertRad2Px(p3);
        map->convertRad2Px(p4);

        QPolygonF poly;
        poly << p1 << p2 << p3 << p4;
        p.setPen(Qt::black);
        p.drawPolygon(poly);
#endif // DEBUG_SHOW_SUBDIV_BORDERS

#ifdef Q_OS_WIN32
        file.close();
#else
        file.free();
#endif
    }

#ifndef Q_OS_WIN32
    file.close();
#endif
}

void CMapIMG::loadSubDiv(CFileExt &file, const CGarminSubfile::subdiv_desc_t& subdiv, const IGarminStrTbl * strtbl, const QByteArray& rgndata, bool fast, const QRectF& viewport, polytype_t& polylines, polytype_t& polygons, pointtype_t& points, pointtype_t& pois)
{
    if(subdiv.rgn_start == subdiv.rgn_end && !subdiv.lengthPolygons2 && !subdiv.lengthPolylines2 && !subdiv.lengthPoints2)
    {
        return;
    }
    //fprintf(stderr, "loadSubDiv\n");
    //     qDebug() << "---------" << file.fileName() << "---------";

    const quint8 * pRawData = (quint8*)rgndata.data();

    quint32 opnt = 0, oidx = 0, opline = 0, opgon = 0;
    quint32 objCnt = subdiv.hasIdxPoints + subdiv.hasPoints + subdiv.hasPolylines + subdiv.hasPolygons;

    quint16 * pOffset = (quint16*)(pRawData + subdiv.rgn_start);

    // test for points
    if(subdiv.hasPoints)
    {
        opnt = (objCnt - 1) * sizeof(quint16) + subdiv.rgn_start;
    }
    // test for indexed points
    if(subdiv.hasIdxPoints)
    {
        if(opnt)
        {
            oidx = gar_load(uint16_t, *pOffset);
            oidx += subdiv.rgn_start;
            ++pOffset;
        }
        else
        {
            oidx = (objCnt - 1) * sizeof(quint16) + subdiv.rgn_start;
        }
    }
    // test for polylines
    if(subdiv.hasPolylines)
    {
        if(opnt || oidx)
        {
            opline = gar_load(uint16_t, *pOffset);
            opline += subdiv.rgn_start;
            ++pOffset;
        }
        else
        {
            opline = (objCnt - 1) * sizeof(quint16) + subdiv.rgn_start;
        }
    }
    // test for polygons
    if(subdiv.hasPolygons)
    {
        if(opnt || oidx || opline)
        {
            opgon = gar_load(uint16_t, *pOffset);
            opgon += subdiv.rgn_start;
            ++pOffset;
        }
        else
        {
            opgon = (objCnt - 1) * sizeof(quint16) + subdiv.rgn_start;
        }
    }

#ifdef DEBUG_SHOW_POLY_DATA
    qDebug() << "--- Subdivision" << subdiv.n << "---";
    qDebug() << "address:" << hex << subdiv.rgn_start << "- " << subdiv.rgn_end;
    qDebug() << "points:            " << hex << opnt;
    qDebug() << "indexed points:    " << hex << oidx;
    qDebug() << "polylines:         " << hex << opline;
    qDebug() << "polygons:          " << hex << opgon;
#endif                       // DEBUG_SHOW_POLY_DATA

    CGarminPolygon p;

    // decode points
    if(subdiv.hasPoints && !fast && getShowPOIs())
    {
        const quint8 *pData = pRawData + opnt;
        const quint8 *pEnd  = pRawData + (oidx ? oidx : opline ? opline : opgon ? opgon : subdiv.rgn_end);
        while(pData < pEnd)
        {
            CGarminPoint p;
            pData += p.decode(subdiv.iCenterLng, subdiv.iCenterLat, subdiv.shift, pData);

            // skip points outside our current viewport
            if(!viewport.contains(p.pos))
            {
                continue;
            }

            if(strtbl)
            {
                p.isLbl6 ? strtbl->get(file, p.lbl_ptr, IGarminStrTbl::poi, p.labels)
                : strtbl->get(file, p.lbl_ptr, IGarminStrTbl::norm, p.labels);
            }

            points.push_back(p);
        }
    }

    // decode indexed points
    if(subdiv.hasIdxPoints && !fast && getShowPOIs())
    {
        const quint8 *pData = pRawData + oidx;
        const quint8 *pEnd  = pRawData + (opline ? opline : opgon ? opgon : subdiv.rgn_end);
        while(pData < pEnd)
        {
            CGarminPoint p;
            pData += p.decode(subdiv.iCenterLng, subdiv.iCenterLat, subdiv.shift, pData);

            // skip points outside our current viewport
            if(!viewport.contains(p.pos))
            {
                continue;
            }

            if(strtbl)
            {
                p.isLbl6 ? strtbl->get(file, p.lbl_ptr, IGarminStrTbl::poi, p.labels)
                : strtbl->get(file, p.lbl_ptr, IGarminStrTbl::norm, p.labels);
            }

            pois.push_back(p);
        }
    }

    // decode polylines
    if(subdiv.hasPolylines && !fast && getShowPolylines())
    {
        CGarminPolygon::cnt = 0;
        const quint8 *pData = pRawData + opline;
        const quint8 *pEnd  = pRawData + (opgon ? opgon : subdiv.rgn_end);
        while(pData < pEnd)
        {
            pData += p.decode(subdiv.iCenterLng, subdiv.iCenterLat, subdiv.shift, true, pData, pEnd);

            // skip points outside our current viewport
            if(isCompletelyOutside(p.pixel, viewport))
            {
                continue;
            }

            if(strtbl && !p.lbl_in_NET && p.lbl_info)
            {
                strtbl->get(file, p.lbl_info, IGarminStrTbl::norm, p.labels);
            }
            else if(strtbl && p.lbl_in_NET && p.lbl_info)
            {
                strtbl->get(file, p.lbl_info, IGarminStrTbl::net, p.labels);
            }

            polylines.push_back(p);
        }
    }

    // decode polygons
    if(subdiv.hasPolygons && getShowPolygons())
    {
        CGarminPolygon::cnt = 0;
        const quint8 *pData = pRawData + opgon;
        const quint8 *pEnd  = pRawData + subdiv.rgn_end;

        while(pData < pEnd)
        {
            pData += p.decode(subdiv.iCenterLng, subdiv.iCenterLat, subdiv.shift, false, pData, pEnd);

            // skip points outside our current viewport
            if(isCompletelyOutside(p.pixel, viewport))
            {
                continue;
            }

            if(strtbl && !p.lbl_in_NET && p.lbl_info && !fast)
            {
                strtbl->get(file, p.lbl_info, IGarminStrTbl::norm, p.labels);
            }
            else if(strtbl && p.lbl_in_NET && p.lbl_info && !fast)
            {
                strtbl->get(file, p.lbl_info, IGarminStrTbl::net, p.labels);
            }
            polygons.push_back(p);
        }
    }

    //         qDebug() << "--- Subdivision" << subdiv.n << "---";
    //         qDebug() << "adress:" << hex << subdiv.rgn_start << "- " << subdiv.rgn_end;
    //         qDebug() << "polyg off: " << hex << subdiv.offsetPolygons2;
    //         qDebug() << "polyg len: " << hex << subdiv.lengthPolygons2 << dec << subdiv.lengthPolygons2;
    //         qDebug() << "polyg end: " << hex << subdiv.lengthPolygons2 + subdiv.offsetPolygons2;
    //         qDebug() << "polyl off: " << hex << subdiv.offsetPolylines2;
    //         qDebug() << "polyl len: " << hex << subdiv.lengthPolylines2 << dec << subdiv.lengthPolylines2;
    //         qDebug() << "polyl end: " << hex << subdiv.lengthPolylines2 + subdiv.offsetPolylines2;
    //         qDebug() << "point off: " << hex << subdiv.offsetPoints2;
    //         qDebug() << "point len: " << hex << subdiv.lengthPoints2 << dec << subdiv.lengthPoints2;
    //         qDebug() << "point end: " << hex << subdiv.lengthPoints2 + subdiv.offsetPoints2;

    if(subdiv.lengthPolygons2 && getShowPolygons())
    {
        const quint8 *pData   = pRawData + subdiv.offsetPolygons2;
        const quint8 *pEnd    = pData + subdiv.lengthPolygons2;
        while(pData < pEnd)
        {
            //             qDebug() << "rgn offset:" << hex << (rgnoff + (pData - pRawData));
            pData += p.decode2(subdiv.iCenterLng, subdiv.iCenterLat, subdiv.shift, false, pData, pEnd);

            // skip points outside our current viewport
            if(isCompletelyOutside(p.pixel, viewport))
            {
                continue;
            }

            if(strtbl && !p.lbl_in_NET && p.lbl_info && !fast)
            {
                strtbl->get(file, p.lbl_info, IGarminStrTbl::norm, p.labels);
            }

            polygons.push_back(p);
        }
    }

    if(subdiv.lengthPolylines2 && !fast && getShowPolylines())
    {
        const quint8 *pData = pRawData + subdiv.offsetPolylines2;
        const quint8 *pEnd  = pData + subdiv.lengthPolylines2;
        while(pData < pEnd)
        {
            //             qDebug() << "rgn offset:" << hex << (rgnoff + (pData - pRawData));
            pData += p.decode2(subdiv.iCenterLng, subdiv.iCenterLat, subdiv.shift, true, pData, pEnd);

            // skip points outside our current viewport
            if(isCompletelyOutside(p.pixel, viewport))
            {
                continue;
            }

            if(strtbl && !p.lbl_in_NET && p.lbl_info)
            {
                strtbl->get(file, p.lbl_info, IGarminStrTbl::norm, p.labels);
            }

            polylines.push_back(p);
        }
    }

    if(subdiv.lengthPoints2 && !fast && getShowPOIs())
    {
        const quint8 *pData   = pRawData + subdiv.offsetPoints2;
        const quint8 *pEnd    = pData + subdiv.lengthPoints2;
        while(pData < pEnd)
        {
            CGarminPoint p;
            //             qDebug() << "rgn offset:" << hex << (rgnoff + (pData - pRawData));
            pData += p.decode2(subdiv.iCenterLng, subdiv.iCenterLat, subdiv.shift, pData, pEnd);

            // skip points outside our current viewport
            if(!viewport.contains(p.pos))
            {
                continue;
            }

            if(strtbl)
            {
                p.isLbl6 ? strtbl->get(file, p.lbl_ptr, IGarminStrTbl::poi, p.labels)
                : strtbl->get(file, p.lbl_ptr, IGarminStrTbl::norm, p.labels);
            }
            pois.push_back(p);
        }
    }
}

void CMapIMG::drawPolygons(QPainter& p, polytype_t& lines)
{
    const int N = polygonDrawOrder.size();
    for(int n = 0; n < N; ++n)
    {
        quint32 type = polygonDrawOrder[(N - 1) - n];

        p.setPen(polygonProperties[type].pen);
        p.setBrush(CMainWindow::self().isNight() ? polygonProperties[type].brushNight : polygonProperties[type].brushDay);

        for(CGarminPolygon &line : lines)
        {
            if(line.type != type)
            {
                continue;
            }

            QPolygonF &poly = line.pixel;

            map->convertRad2Px(poly);

//            simplifyPolyline(line);

            p.drawPolygon(poly);

            if(!polygonProperties[type].known)
            {
                qDebug() << "unknown polygon" << hex << type;
            }
        }
    }
}


void CMapIMG::drawPolylines(QPainter& p, polytype_t& lines, const QPointF& scale)
{
    textpaths.clear();
    QFont font = CMainWindow::self().getMapFont();

    font.setPixelSize(12);
    font.setBold(false);
    QFontMetricsF metrics(font);

    QVector<qreal> lengths;
    lengths.reserve(100);
/*
    int pixmapCount = 0;
    int borderCount = 0;
    int normalCount = 0;
    int imageCount = 0;
    int deletedCount = 0;
 */

    QHash<quint32, QList<quint32> > dict;
    for(int i = 0; i < lines.count(); ++i)
    {
        dict[lines[i].type].push_back(i);
    }

    QMap<quint32, CGarminTyp::polyline_property>::iterator props = polylineProperties.begin();
    QMap<quint32, CGarminTyp::polyline_property>::iterator end = polylineProperties.end();
    for(; props != end; ++props)
    {
        const quint32 &type = props.key();
        const CGarminTyp::polyline_property& property = props.value();

        if(dict[type].isEmpty())
        {
            continue;
        }

        if(property.hasPixmap)
        {
            const QImage &pixmap = CMainWindow::self().isNight() ? property.imgNight : property.imgDay;
            const qreal h        = pixmap.height();

            QList<quint32>::const_iterator it = dict[type].constBegin();
            for(; it != dict[type].constEnd(); ++it)
            {
                CGarminPolygon &item = lines[*it];
                {
                    //pixmapCount++;

                    QPolygonF& poly = item.pixel;
                    int size        = poly.size();

                    if(size < 2)
                    {
                        continue;
                    }

                    map->convertRad2Px(poly);

                    lengths.resize(0);


//                    deletedCount += line.size();
//                    simplifyPolyline(line);
//                    deletedCount -= line.size();
//                    size = line.size();

                    lengths.reserve(size);

                    QPainterPath path;
                    qreal totalLength = 0;

                    qreal u1 = poly[0].x();
                    qreal v1 = poly[0].y();

                    for(int i = 1; i < size; ++i)
                    {
                        qreal u2 = poly[i].x();
                        qreal v2 = poly[i].y();

                        qreal segLength = qSqrt((u2 - u1) * (u2 - u1) + (v2 - v1) * (v2 - v1));
                        totalLength += segLength;
                        lengths << segLength;

                        u1 = u2;
                        v1 = v2;
                    }

                    if (scale.x() < STREETNAME_THRESHOLD && property.labelType != CGarminTyp::eNone)
                    {
                        collectText(item, poly, font, metrics, h);
                    }

                    path.addPolygon(poly);
                    const int nLength = lengths.count();

                    qreal curLength = 0;
                    QPointF p2       = path.pointAtPercent(curLength / totalLength);
                    for(int i = 0; i < nLength; ++i)
                    {
                        qreal segLength = lengths.at(i);

                        //                         qDebug() << curLength << totalLength << curLength / totalLength;

                        QPointF p1  = p2;
                        p2          = path.pointAtPercent((curLength + segLength) / totalLength);
                        qreal angle = qAtan((p2.y() - p1.y()) / (p2.x() - p1.x())) * 180 / M_PI;

                        if(p2.x() - p1.x() < 0)
                        {
                            angle += 180;
                        }

                        p.save();
                        p.translate(p1);
                        p.rotate(angle);
                        p.drawImage(0, -h / 2, img2line(pixmap, segLength));
                        //imageCount++;

                        p.restore();
                        curLength += segLength;
                    }
                }
            }
        }
        else
        {
            if(property.hasBorder)
            {
                // draw background line 1st
                p.setPen(CMainWindow::self().isNight() ? property.penBorderNight : property.penBorderDay);

                QList<quint32>::const_iterator it = dict[type].constBegin();
                for(; it != dict[type].constEnd(); ++it)
                {
                    //borderCount++;
                    drawLine(p, lines[*it], property, metrics, font, scale);
                }
                // draw foreground line in a second run for nicer borders
            }
            else
            {
                p.setPen(CMainWindow::self().isNight() ? property.penLineNight : property.penLineDay);

                QList<quint32>::const_iterator it = dict[type].constBegin();
                for(; it != dict[type].constEnd(); ++it)
                {
                    //normalCount++;
                    drawLine(p, lines[*it], property, metrics, font, scale);
                }
            }
        }
    }

    // 2nd run to draw foreground lines.
    props = polylineProperties.begin();
    for(; props != end; ++props)
    {
        const quint32 &type = props.key();
        const CGarminTyp::polyline_property& property = props.value();

        if(dict[type].isEmpty())
        {
            continue;
        }

        if(property.hasBorder && !property.hasPixmap)
        {
            // draw foreground line 2nd
            p.setPen(CMainWindow::self().isNight() ? property.penLineNight : property.penLineDay);

            QList<quint32>::const_iterator it = dict[type].constBegin();
            for(; it != dict[type].constEnd(); ++it)
            {
                drawLine(p, lines[*it]);
            }
        }
    }

    //    qDebug() << "pixmapCount:" << pixmapCount
    //        << "borderCount:" << borderCount
    //        << "normalCount:" << normalCount
    //        << "imageCount:" << imageCount
    //        << "deletedCount:" << deletedCount;
}

void CMapIMG::drawLine(QPainter& p, CGarminPolygon& l, const CGarminTyp::polyline_property& property, const QFontMetricsF& metrics, const QFont& font, const QPointF& scale)
{
    QPolygonF& poly     = l.pixel;
    const int size      = poly.size();
    const int lineWidth = p.pen().width();

    if(size < 2)
    {
        return;
    }

    map->convertRad2Px(poly);

//    simplifyPolyline(line);

    if (scale.x() < STREETNAME_THRESHOLD && property.labelType != CGarminTyp::eNone)
    {
        collectText(l, poly, font, metrics, lineWidth);
    }

    p.drawPolyline(poly);
}

void CMapIMG::drawLine(QPainter& p, const CGarminPolygon& l)
{
    const QPolygonF& poly   = l.pixel;
    const int size          = poly.size();

    if(size < 2)
    {
        return;
    }


//    simplifyPolyline(poly);

    p.drawPolyline(poly);
}



void CMapIMG::collectText(const CGarminPolygon& item, const QPolygonF& line, const QFont& font, const QFontMetricsF& metrics, qint32 lineWidth)
{
    QString str;
    if(item.hasLabel())
    {
        str = item.getLabelText();
    }

    if(str.isEmpty())
    {
        return;
    }

    textpath_t tp;
    tp.polyline  = line;
    tp.font      = font;
    tp.text      = str;
    tp.lineWidth = lineWidth;

    const int size = line.size();
    for(int i = 1; i < size; ++i)
    {
        const QPointF &p1 = line[i - 1];
        const QPointF &p2 = line[i];
        qreal dx = p2.x() - p1.x();
        qreal dy = p2.y() - p1.y();
        tp.lengths << qSqrt(dx * dx + dy * dy);
    }

    textpaths << tp;
}

bool CMapIMG::intersectsWithExistingLabel(const QRect &rect) const
{
    for(const strlbl_t &label : labels)
    {
        if(label.rect.intersects(rect))
        {
            return true;
        }
    }

    return false;
}

void CMapIMG::addLabel(const CGarminPoint &pt, const QRect &rect, CGarminTyp::label_type_e type)
{
    QString str;
    if(pt.hasLabel())
    {
        str = pt.getLabelText();
    }

    labels.push_back(strlbl_t());
    strlbl_t& strlbl = labels.last();
    strlbl.pt   = pt.pos.toPoint();
    strlbl.str  = str;
    strlbl.rect = rect;
    strlbl.type = type;
}

void CMapIMG::drawPoints(QPainter& p, pointtype_t& pts, QVector<QRectF>& rectPois)
{
    pointtype_t::iterator pt = pts.begin();
    while(pt != pts.end())
    {
//        if((pt->type > 0x1600) && (zoomFactor > CResources::self().getZoomLevelThresholdPois()))
//        {
//            ++pt;
//            continue;
//        };

        map->convertRad2Px(pt->pos);

        const QImage&  icon = CMainWindow::self().isNight() ? pointProperties[pt->type].imgNight : pointProperties[pt->type].imgDay;
        const QSizeF&  size = icon.size();

        if(isCluttered(rectPois, QRectF(pt->pos, size)))
        {
            if(size.width() <= 8 && size.height() <= 8)
            {
                p.drawImage(pt->pos.x() - (size.width() / 2), pt->pos.y() - (size.height() / 2), icon);
            }
            else
            {
                p.drawPixmap(pt->pos.x() - 4, pt->pos.y() - 4, QPixmap(":/icons/8x8/bullet_blue.png"));
            }
            ++pt;
            continue;
        }

        bool showLabel = true;

        if(pointProperties.contains(pt->type))
        {
            p.drawImage(pt->pos.x() - (size.width() / 2), pt->pos.y() - (size.height() / 2), icon);
            showLabel = pointProperties[pt->type].labelType != CGarminTyp::eNone;
        }
        else
        {
            p.drawPixmap(pt->pos.x() - 4, pt->pos.y() - 4, QPixmap(":/icons/8x8/bullet_blue.png"));
        }

        if(CMainWindow::self().isPOIText() && showLabel)
        {
            // calculate bounding rectangle with a border of 2 px
            QRect rect = fm.boundingRect(pt->labels.join(" "));
            rect.adjust(0, 0, 4, 4);
            rect.moveCenter(pt->pos.toPoint());

            // if no intersection was found, add label to list
            if(!intersectsWithExistingLabel(rect))
            {
                addLabel(*pt, rect, CGarminTyp::eStandard);
            }
        }
        ++pt;
    }
}


void CMapIMG::drawPois(QPainter& p, pointtype_t& pts, QVector<QRectF> &rectPois)
{
    CGarminTyp::label_type_e labelType = CGarminTyp::eStandard;

    for(CGarminPoint &pt : pts)
    {
        map->convertRad2Px(pt.pos);

        const QImage&  icon = CMainWindow::self().isNight() ? pointProperties[pt.type].imgNight : pointProperties[pt.type].imgDay;
        const QSizeF&  size = icon.size();

        if(isCluttered(rectPois, QRectF(pt.pos, size)))
        {
            if(size.width() <= 8 && size.height() <= 8)
            {
                p.drawImage(pt.pos.x() - (size.width() / 2), pt.pos.y() - (size.height() / 2), icon);
            }
            else
            {
                p.drawPixmap(pt.pos.x() - 4, pt.pos.y() - 4, QPixmap(":/icons/8x8/bullet_blue.png"));
            }
            continue;
        }

        labelType = CGarminTyp::eStandard;
        if(pointProperties.contains(pt.type))
        {
            p.drawImage(pt.pos.x() - (size.width() / 2), pt.pos.y() - (size.height() / 2), icon);
            labelType = pointProperties[pt.type].labelType;
        }
        else
        {
            p.drawPixmap(pt.pos.x() - 4, pt.pos.y() - 4, QPixmap(":/icons/8x8/bullet_red.png"));
        }

        if(CMainWindow::self().isPOIText())
        {
            // calculate bounding rectangle with a border of 2 px
            QRect rect = fm.boundingRect(pt.labels.join(" "));
            rect.adjust(0, 0, 4, 4);
            rect.moveCenter(pt.pos.toPoint());

            // if no intersection was found, add label to list
            if(!intersectsWithExistingLabel(rect))
            {
                addLabel(pt, rect, labelType);
            }
        }
    }
}


void CMapIMG::drawLabels(QPainter& p, const QVector<strlbl_t> &lbls)
{
    QFont f = CMainWindow::self().getMapFont();
    QVector<QFont> fonts(8, f);
    fonts[CGarminTyp::eSmall].setPointSize(f.pointSize() - 2);
    fonts[CGarminTyp::eLarge].setPointSize(f.pointSize() + 2);

    for(const strlbl_t &lbl : lbls)
    {
        CDraw::text(lbl.str, p, lbl.pt, Qt::black, fonts[lbl.type]);
    }
}

void CMapIMG::drawText(QPainter& p)
{
    p.setPen(Qt::black);

    for(const textpath_t &textpath : textpaths)
    {
        QPainterPath path;
        QFont font = textpath.font;
        QFontMetricsF fm(font);

        path.addPolygon(textpath.polyline);

        // get path length and string length
        qreal length = qAbs(path.length());
        qreal width  = fm.width(textpath.text);

        // adjust font size until string fits into polyline
        while(width > (length * 0.7))
        {
            font.setPixelSize(font.pixelSize() - 1);
            fm      = QFontMetricsF(font);
            width   = fm.width(textpath.text);

            if((font.pixelSize() < 8))
            {
                break;
            }
        }

        // no way to draw a readable string - skip
        if((font.pixelSize() < 8))
        {
            continue;
        }

        fm = QFontMetricsF(font);
        p.setFont(font);

        // adjust exact offset to first half of segment
        const QVector<qreal>& lengths = textpath.lengths;
        const qreal ref = (length - width) / 2;
        qreal offset    = 0;

        for(int i = 0; i < lengths.size(); ++i)
        {
            const qreal d = lengths[i];

            if((offset + d / 2) >= ref)
            {
                offset = ref;
                break;
            }
            if((offset + d) >= ref)
            {
                offset += d / 2;
                break;
            }
            offset += d;
        }

        // get starting angle of first two letters
        const QString& text = textpath.text;
        qreal percent1  =  offset / length;
        qreal percent2  = (offset + fm.width(text.left(2))) / length;

        QPointF point1  = path.pointAtPercent(percent1);
        QPointF point2  = path.pointAtPercent(percent2);

        qreal angle; //     = qAtan((point2.y() - point1.y()) / (point2.x() - point1.x())) * 180 / M_PI;

        // flip path if string start is E->W direction
        // this helps, sometimes, in 50 % of the cases :)
        if(point2.x() - point1.x() < 0)
        {
            path    = path.toReversed();
        }

        // draw string letter by letter and adjust angle
        const int size = text.size();
        percent2 = offset / length;
        point2   = path.pointAtPercent(percent2);

        for(int i = 0; i < size; ++i)
        {
            //percent1 = percent2;
            percent2 = (offset + fm.width(text[i])) / length;

            point1  = point2;
            point2  = path.pointAtPercent(percent2);

            angle   = qAtan((point2.y() - point1.y()) / (point2.x() - point1.x())) * 180 / M_PI;

            if(point2.x() - point1.x() < 0)
            {
                angle += 180;
            }

            p.save();
            p.translate(point1);
            p.rotate(angle);

            p.translate(0, -(textpath.lineWidth + 2));

            QString str = text.mid(i, 1);
            p.setPen(Qt::white);
            p.drawText(-1, -1, str);
            p.drawText( 0, -1, str);
            p.drawText(+1, -1, str);
            p.drawText(-1, 0, str);
            p.drawText(+1, 0, str);
            p.drawText(-1, +1, str);
            p.drawText( 0, +1, str);
            p.drawText(+1, +1, str);

            p.setPen(Qt::black);
            p.drawText( 0, 0, str);

            p.restore();

            offset += fm.width(text[i]);
        }
    }
}

void CMapIMG::getToolTip(const QPoint& px, QString& infotext) const /* override */
{
    QString str;

    QMultiMap<QString, QString> dict;
    getInfoPoints(points, px, dict);
    getInfoPoints(pois,   px, dict);
    getInfoPolylines(px, dict);

    for(const QString &value : dict.values())
    {
        if(value == "-")
        {
            continue;
        }

        if(!str.isEmpty())
        {
            str += "\n";
        }
        str += value;
    }

    if(str.isEmpty())
    {
        dict.clear();
        getInfoPolygons(px, dict);
        for(const QString &value : dict.values())
        {
            if(value == "-")
            {
                continue;
            }

            if(!str.isEmpty())
            {
                str += "\n";
            }
            str += value;
        }
    }

    if(!infotext.isEmpty() && !str.isEmpty())
    {
        infotext += "\n" + str;
    }
    else
    {
        infotext += str;
    }
}

void CMapIMG::findPOICloseBy(const QPoint& pt, poi_t& poi) const /*override;*/
{
    for(auto &list : {points, pois})
    {
        for(const CGarminPoint &point : list)
        {
            QPoint x = pt - QPoint(point.pos.x(), point.pos.y());
            if(x.manhattanLength() < 10)
            {
                poi.pos = point.pos;
                if(!point.labels.isEmpty())
                {
                    poi.name  = point.labels.first();
                    poi.desc  = point.getLabelText();
                }
                else
                {
                    if(pointProperties.contains(point.type))
                    {
                        poi.name = pointProperties[point.type].strings[selectedLanguage != NOIDX ? selectedLanguage : 0];
                    }
                    else
                    {
                        poi.name = QString(" (%1)").arg(point.type, 2, 16, QChar('0'));
                    }
                }
                if(pointProperties.contains(point.type))
                {
                    poi.symbolSize = pointProperties[point.type].imgDay.size();
                }
                else
                {
                    poi.symbolSize = QSize(16, 16);
                }
                return;
            }
        }
    }
}

void CMapIMG::getInfoPoints(const pointtype_t &points, const QPoint& pt, QMultiMap<QString, QString>& dict) const
{
    for(const CGarminPoint &point : points)
    {
        QPoint x = pt - QPoint(point.pos.x(), point.pos.y());
        if(x.manhattanLength() < 10)
        {
            if(point.hasLabel())
            {
                dict.insert(tr("Point of Interest"), point.getLabelText());
            }
            else
            {
                if(pointProperties.contains(point.type))
                {
                    dict.insert(tr("Point of Interest"), pointProperties[point.type].strings[selectedLanguage != NOIDX ? selectedLanguage : 0]);
                }
                else
                {
                    dict.insert(tr("Point of Interest"), QString(" (%1)").arg(point.type, 2, 16, QChar('0')));
                }
            }
        }
    }
}

void CMapIMG::getInfoPolylines(const QPoint &pt, QMultiMap<QString, QString>& dict) const
{
    projXY p1, p2;              // the two points of the polyline close to pt
    qreal u;                    // ratio u the tangent point will divide d_p1_p2
    qreal shortest = 20;        // shortest distance so far

    QPointF resPt = pt;
    QString key;
    QStringList value;
    quint32 type = 0;

    bool found = false;

    for(const CGarminPolygon &line : polylines)
    {
        int len = line.pixel.size();
        // need at least 2 points
        if(len < 2)
        {
            continue;
        }

        // see http://local.wasp.uwa.edu.au/~pbourke/geometry/pointline/
        for(int i = 1; i < len; ++i)
        {
            p1.u = line.pixel[i - 1].x();
            p1.v = line.pixel[i - 1].y();
            p2.u = line.pixel[i].x();
            p2.v = line.pixel[i].y();

            qreal dx = p2.u - p1.u;
            qreal dy = p2.v - p1.v;

            // distance between p1 and p2
            qreal d_p1_p2 = qSqrt(dx * dx + dy * dy);

            u = ((pt.x() - p1.u) * dx + (pt.y() - p1.v) * dy) / (d_p1_p2 * d_p1_p2);

            if(u < 0.0 || u > 1.0)
            {
                continue;
            }

            // coord. (x,y) of the point on line defined by [p1,p2] close to pt
            qreal x = p1.u + u * dx;
            qreal y = p1.v + u * dy;

            qreal distance = qSqrt((x - pt.x()) * (x - pt.x()) + (y - pt.y()) * (y - pt.y()));

            if(distance < shortest)
            {
                type  = line.type;
                value.clear();
                value << (line.hasLabel() ? line.getLabelText() : "-");

                resPt.setX(x);
                resPt.setY(y);
                shortest = distance;
                found = true;
            }
            else if(distance == shortest)
            {
                if(line.hasLabel())
                {
                    value << line.getLabelText();
                }
            }
        }
    }

    value.removeDuplicates();

    if(!found)
    {
        return;
    }

    if(selectedLanguage != NOIDX)
    {
        key =  polylineProperties[type].strings[selectedLanguage];
    }

    if(!key.isEmpty())
    {
        dict.insert(key + QString("(%1)").arg(type, 2, 16, QChar('0')), value.join("\n"));
    }
    else
    {
        if(polylineProperties[type].strings.isEmpty())
        {
            dict.insert(tr("Unknown") + QString("(%1)").arg(type, 2, 16, QChar('0')), value.join("\n"));
        }
        else
        {
            dict.insert(polylineProperties[type].strings[0] + QString("(%1)").arg(type, 2, 16, QChar('0')), value.join("\n"));
        }
    }

//    pt = resPt.toPoint();
}

void CMapIMG::getInfoPolygons(const QPoint& pt, QMultiMap<QString, QString>& dict) const
{
    projXY p1, p2;               // the two points of the polyline close to pt
    const qreal x = pt.x();
    const qreal y = pt.y();

    for(const CGarminPolygon &line : polygons)
    {
        int npol = line.pixel.size();
        if(npol > 2)
        {
            bool c = false;
            // see http://local.wasp.uwa.edu.au/~pbourke/geometry/insidepoly/
            for (int i = 0, j = npol - 1; i < npol; j = i++)
            {
                p1.u = line.pixel[j].x();
                p1.v = line.pixel[j].y();
                p2.u = line.pixel[i].x();
                p2.v = line.pixel[i].y();

                if ((((p2.v <= y) && (y < p1.v))  || ((p1.v <= y) && (y < p2.v))) &&
                    (x < (p1.u - p2.u) * (y - p2.v) / (p1.v - p2.v) + p2.u))
                {
                    c = !c;
                }
            }

            if(c)
            {
                if(line.labels.size())
                {
                    dict.insert(tr("Area"), line.labels.join(" ").simplified());
                }
                else
                {
                    if(selectedLanguage != NOIDX)
                    {
                        if(polygonProperties[line.type].strings[selectedLanguage].size())
                        {
                            dict.insert(tr("Area"), polygonProperties[line.type].strings[selectedLanguage]);
                        }
                    }
                    else
                    {
                        if(polygonProperties[line.type].strings[0].size())
                        {
                            dict.insert(tr("Area"), polygonProperties[line.type].strings[0]);
                        }
                    }
                }
            }
        }
    }
}




bool CMapIMG::findPolylineCloseBy(const QPointF& pt1, const QPointF& pt2, qint32 threshold, QPolygonF& polyline) /* override */
{
    for(const CGarminPolygon &line : polylines)
    {
        if(line.pixel.size() < 2)
        {
            continue;
        }
        if(0x20 <= line.type && line.type <= 0x25)
        {
            continue;
        }

        qreal dist1 = GPS_Math_DistPointPolyline(line.pixel, pt1, threshold);
        qreal dist2 = GPS_Math_DistPointPolyline(line.pixel, pt2, threshold);

        if(dist1 < threshold && dist2 < threshold)
        {
            polyline  = line.coords;
            threshold = qMin(dist1, dist2);
        }
    }

    return !polyline.isEmpty();
}
