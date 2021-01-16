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

#ifndef CMAPIMG_H
#define CMAPIMG_H

#include "helpers/CFileExt.h"
#include "helpers/Platform.h"
#include "map/garmin/CGarminPoint.h"
#include "map/garmin/CGarminPolygon.h"
#include "map/garmin/CGarminSubfile.h"
#include "map/garmin/CGarminTyp.h"
#include "map/garmin/Garmin.h"
#include "map/IMap.h"

#include <QMap>

class CMapDraw;
class CFileExt;
class IGarminStrTbl;

typedef QVector<CGarminPolygon> polytype_t;
typedef QVector<CGarminPoint> pointtype_t;


class CMapIMG : public IMap
{
    Q_OBJECT
public:
    CMapIMG(const QString &filename, CMapDraw *parent);
    virtual ~CMapIMG() = default;

    void loadConfig(QSettings& cfg) override;

    void draw(IDrawContext::buffer_t& buf) override;

    void getToolTip(const QPoint& px, QString& infotext) const override;

    void findPOICloseBy(const QPoint&, poi_t& poi) const override;

    /**
       @brief Find a matching street polyline

       The polyline must be close enough in terms of pixel to point 1 and 2. "Close enough" is defined by
       the threshold. The returned polyline uses lon/lat as coordinates.

       @param pt1           first point in [rad]
       @param pt2           second point in [rad]
       @param threshold     the "close enough" threshold in [pixel]
       @param polyline      the resulting polyline, if any, in [rad]
       @return              Return true if a line has been found.
     */
    bool findPolylineCloseBy(const QPointF &pt1, const QPointF &pt2, qint32 threshold, QPolygonF& polyline) override;

    static void readFile(CFileExt& file, quint32 offset, quint32 size, QByteArray& data);

public slots:
    void slotSetTypeFile(const QString& filename) override;

private:
    struct strlbl_t
    {
        QPoint pt;
        QRect rect;
        QString str;
        CGarminTyp::label_type_e type = CGarminTyp::eStandard;
    };


    quint8 scale2bits(const QPointF &scale);
    void setupTyp();
    void readBasics();
    void readSubfileBasics(CGarminSubfile& subfile, CFileExt &file);
    void processPrimaryMapData();
    void loadVisibleData(bool fast, polytype_t& polygons, polytype_t& polylines, pointtype_t& points, pointtype_t& pois, unsigned level, const QRectF& viewport, QPainter& p);
    void loadSubDiv(CFileExt &file, const CGarminSubfile::subdiv_desc_t& subdiv, const CGarminSubfile& subfile, bool fast, const QRectF& viewport, polytype_t& polylines, polytype_t& polygons, pointtype_t& points, pointtype_t& pois);
    bool intersectsWithExistingLabel(const QRect &rect) const;
    void addLabel(const CGarminPoint &pt, const QRect &rect, CGarminTyp::label_type_e type);
    void drawPolygons(QPainter& p, polytype_t& lines);
    void drawPolylines(QPainter& p, polytype_t& lines, const QPointF &scale);
    void drawPoints(QPainter& p, pointtype_t& pts, QVector<QRectF> &rectPois);
    void drawPois(QPainter& p, pointtype_t& pts, QVector<QRectF>& rectPois);
    void drawLabels(QPainter& p, const QVector<strlbl_t> &lbls);
    void drawText(QPainter& p);

    void drawLine(QPainter& p, CGarminPolygon& l, const CGarminTyp::polyline_property& property, const QFontMetricsF& metrics, const QFont& font, const QPointF& scale);
    void drawLine(QPainter& p, const CGarminPolygon& l);

    void collectText(const CGarminPolygon& item, const QPolygonF& line, const QFont& font, const QFontMetricsF& metrics, qint32 lineWidth);

    void getInfoPoints(const pointtype_t &points, const QPoint& pt, QMultiMap<QString, QString>& dict) const;
    void getInfoPolylines(const QPoint& pt, QMultiMap<QString, QString>& dict) const;
    void getInfoPolygons(const QPoint& pt, QMultiMap<QString, QString>& dict) const;

    template<typename T>
    void readSubfileHeader(CFileExt& file, quint32 offset, QByteArray& hdr)
    {
        quint16 size = qMin(gar_load(quint16, *(quint16*)file.data(offset, sizeof(quint16))), quint16(sizeof(T)));
        readFile(file, offset, size, hdr);

        qint32 gap = sizeof(T) -  size;
        if(gap)
        {
            hdr += QByteArray(gap, '\0');
        }
    }

#pragma pack(1)
    // Garmin IMG file header structure, to the start of the FAT blocks
    struct hdr_img_t
    {
        quint8 xorByte = 0;                 ///< 0x00000000
        quint8 byte0x00000001_0x00000007[7] = {0};
        quint16 version = 2;                ///< 0x00000008
        quint8 upMonth = 0;                 ///< 0x0000000A
        quint8 upYear = 0;                  ///< 0x0000000B
        quint8 byte0x0000000C_0x0000000D[2] = {0};
        quint8 supp = 0;                    ///< 0x0000000E
        quint8 checksum = 0;                ///< 0x0000000F
        char signature[7] = {0x44, 0x53, 0x4B, 0x49, 0x4D, 0x47, 0x00}; ///< 0x00000010 .. 0x00000016
        quint8 byte0x00000017 = 0x2;
        quint16 sectors1 = 0;               ///< 0x00000018 .. 0x00000019
        quint16 heads1 = 0;                 ///< 0x0000001A .. 0x0000001B
        quint16 cylinders = 0;              ///< 0x0000001C .. 0x0000001D
        quint8 byte0x0000001E_0x000000038[27] = {0};
        qint16 year = 0;                    ///< 0x00000039 .. 0x0000003A
        qint8 month = 0;                    ///< 0x0000003B
        qint8 day = 0;                      ///< 0x0000003C
        qint8 hour = 0;                     ///< 0x0000003D
        qint8 minute = 0;                   ///< 0x0000003E
        qint8 second = 0;                   ///< 0x0000003F
        qint8 offsetFAT = 2;                ///< 0x00000040 offset of the FAT in multiple of 0x200
        char identifier[7] = {0x47, 0x41, 0x52, 0x4D, 0x49, 0x4E, 0x00}; ///< 0x00000041 .. 0x00000047
        quint8 byte0x00000048 = 0;
        char desc1[20] = {0x20};            ///< 0x00000049 .. 0x0000005C
        quint16 heads2 = 0;                 ///< 0x0000005D .. 0x0000005E
        quint16 sectors2 = 0;               ///< 0x0000005F .. 0x00000060
        quint8 e1 = 9;                      ///< 0x00000061
        quint8 e2 = 7;                      ///< 0x00000062
        quint16 nBlocks1 = 0;               ///< 0x00000063 .. 0x00000064
        char desc2[30] = {0x20};            ///< 0x00000065 .. 0x00000082
        quint8 byte0x00000083_0x000001BE[0x13C] = {0};
        quint8 startHead = 0;               ///< 0x000001BF
        quint8 startSector = 1;             ///< 0x000001C0
        quint8 startCylinder = 0;           ///< 0x000001C1
        quint8 systemType = 0;              ///< 0x000001C2
        quint8 endHead = 0;                 ///< 0x000001C3
        quint8 endSector = 0;               ///< 0x000001C4
        quint8 endCylinder = 0;             ///< 0x000001C5
        quint32 relSectors = 0;              ///< 0x000001C6..0x000001C9
        quint32 nSectors = 0;               ///< 0x000001CA .. 0x000001CD
        quint8 byte0x0000001CE_0x000001FD[0x30] = {0};
        quint16 terminator = 0xAA55;        ///< 0x000001FE .. 0x000001FF

        quint32 blocksize()
        {
            return 1 << (e1 + e2);
        }
    };
    struct FATblock_t
    {
        quint8 flag;             ///< 0x00000000
        char name[8];            ///< 0x00000001 .. 0x00000008
        char type[3];            ///< 0x00000009 .. 0x0000000B
        quint32 size;            ///< 0x0000000C .. 0x0000000F
        quint16 part;            ///< 0x00000010 .. 0x00000011
        quint8 byte0x00000012_0x0000001F[14];
        quint16 blocks[240];     ///< 0x00000020 .. 0x000001FF
    };
#ifdef WIN32
#pragma pack()
#else
#pragma pack(0)
#endif
    QString filename;
    QString mapdesc;
    /// hold all subfile descriptors
    /**
        In a normal *.img file there is only one subfile. However
        gmapsupp.img files can hold several subfiles each with it's
        own subfile parts.
     */
    QMap<QString, CGarminSubfile> subfiles;

    QRectF maparea;

    QFontMetrics fm;

    /// combined maplevels of all tiles
    QVector<CGarminSubfile::map_level_t> maplevels;

    QMap<quint32, CGarminTyp::polyline_property> polylineProperties;
    QMap<quint32, CGarminTyp::polygon_property> polygonProperties;
    QList<quint32> polygonDrawOrder;
    QMap<quint32, CGarminTyp::point_property> pointProperties;
    QMap<quint8, QString> languages;

    polytype_t polygons;
    polytype_t polylines;
    pointtype_t points;
    pointtype_t pois;

    QVector<strlbl_t> labels;

    struct textpath_t
    {
        // QPainterPath path;
        QPolygonF polyline;
        QString text;
        QFont font;
        QVector<qreal>  lengths;
        qint32 lineWidth;
    };

    QVector<textpath_t> textpaths;
    qint8 selectedLanguage;
    QSet<QString> copyrights;
};

#endif //CMAPIMG_H

