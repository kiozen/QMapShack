/**********************************************************************************************
    Copyright (C) 2020 Oliver Eichler <oliver.eichler@gmx.de>

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

#ifndef CGARMINSUBFILE_H
#define CGARMINSUBFILE_H

#include "helpers/CFileExt.h"
#include "helpers/Platform.h"
#include "map/garmin/IGarminStrTbl.h"

#include <QtCore>

class CFileExt;

class CGarminSubfile
{
    Q_DECLARE_TR_FUNCTIONS(CGarminSubfile)
public:
    CGarminSubfile();
    virtual ~CGarminSubfile();

    static constexpr auto NO_OFFSET = 0xFFFFFFFF;

    struct part_t
    {
        bool valid() const {return offsetHead != NO_OFFSET;}
        quint32 offsetHead = NO_OFFSET;
        quint32 offsetData = NO_OFFSET;
        quint32 size = 0;
    };

    struct map_level_t
    {
        quint8 bits;
        quint8 level;
        bool inherited;

        bool operator==(const map_level_t &ml)  const
        {
            if (ml.bits != bits || ml.level != level || ml.inherited != inherited)
            {
                return false;
            }
            else
            {
                return true;
            }
        }

        static bool GreaterThan(const map_level_t &ml1, const map_level_t &ml2)
        {
            return ml1.bits < ml2.bits;
        }
    };

    /// subdivision  information
    struct subdiv_desc_t
    {
        quint32 n;

        quint16 next;      //< section of next level
        bool terminate;    //< end of section group
        quint32 rgn_start; //< offset into the subfile's RGN part
        quint32 rgn_end;   //< end of section in RGN part (last offset = rgn_end - 1)


        bool hasPoints;    //< there are points stored in the RGN subsection
        bool hasIdxPoints; //< there are index points stored in the RGN subsection
        bool hasPolylines; //< there are polylines stored in the RGN subsection
        bool hasPolygons;  //< there are polygons stored in the RGN subsection

        qint32 iCenterLng; //< the center longitude of the area covered by this subdivision
        qint32 iCenterLat; //< the center latitude  of the area covered by this subdivision

        qreal north; //< north boundary of area covered by this subsection []
        qreal east;  //< east  boundary of area covered by this subsection []
        qreal south; //< south boundary of area covered by this subsection []
        qreal west;  //< west  boundary of area covered by this subsection []

        /// area in meter coordinates covered by this subdivision []
        QRectF area;

        /// number of left shifts for RGN data
        quint32 shift;
        /// map level this subdivision is shown
        quint32 level;

        quint32 offsetPoints2;
        qint32 lengthPoints2;
        quint32 offsetPolylines2;
        qint32 lengthPolylines2;
        quint32 offsetPolygons2;
        qint32 lengthPolygons2;
    };

    part_t getPart(const QString& name) const;
    void setPart(const QString& name, quint32 offset, quint32 size, CFileExt &file);

    bool isGMP() const {return parts["GMP"].valid();}
    void readBasics(CFileExt& file);

    const QString& getCopyright() const {return copyright;}
    const QRectF& getArea() const {return area;}
    const QVector<map_level_t>& getMaplevels() const {return maplevels;}
    const QVector<subdiv_desc_t>& getSubdivs() const {return subdivs;}
    QByteArray getRgnData(CFileExt& file) const;
    const IGarminStrTbl * getStrtbl() const {return strtbl;}

protected:

    bool transparent = false;

    QMap<QString, part_t> parts = {
        {"TYP", part_t()}
        , {"TRE", part_t()}
        , {"RGN", part_t()}
        , {"LBL", part_t()}
        , {"NET", part_t()}
        , {"NOD", part_t()}
        , {"DEM", part_t()}
    };

    QString copyright;
    qreal northbound = 0;
    qreal eastbound = 0;
    qreal southbound = 0;
    qreal westbound = 0;

    QRectF area;

    QVector<map_level_t> maplevels;
    QVector<subdiv_desc_t> subdivs;

    QPointer<IGarminStrTbl> strtbl;
};

#endif //CGARMINSUBFILE_H

