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

#include "map/CMapException.h"
#include "map/CMapIMG.h"
#include "map/garmin/CGarminStrTbl6.h"
#include "map/garmin/CGarminStrTbl8.h"
#include "map/garmin/CGarminStrTblUtf8.h"
#include "map/garmin/CGarminSubfile.h"
#include "map/garmin/Garmin.h"

template<typename T>
void readSubfileHeader(CFileExt& file, const CGarminSubfile::part_t& part, QByteArray& hdr)
{
    quint16 size = qMin(gar_load(quint16, *(quint16*)file.data(part.offsetHead, sizeof(quint16))), quint16(sizeof(T)));
    CMapIMG::readFile(file, part.offsetHead, size, hdr);

    qint32 gap = sizeof(T) -  size;
    if(gap)
    {
        hdr += QByteArray(gap, '\0');
    }
}


CGarminSubfile::CGarminSubfile()
{
}

CGarminSubfile::~CGarminSubfile()
{
    delete strtbl;
}

QByteArray CGarminSubfile::getRgnData(CFileExt& file) const
{
    QByteArray rgndata;
    const part_t& part = parts["RGN"];
    CMapIMG::readFile(file, part.offsetData, part.size, rgndata);
    return rgndata;
}

CGarminSubfile::part_t CGarminSubfile::getPart(const QString& name) const
{
    Q_ASSERT_X(parts.contains(name), "getPart", "Unknown part ");
    return parts[name];
}

void CGarminSubfile::setPart(const QString& name, quint32 offset, quint32 size, CFileExt &file)
{
    if(parts.contains(name))
    {
        part_t& part = parts[name];
        part.offsetHead = offset;
        part.offsetData = offset;
        part.size = size;
    }
    else if(name == "GMP")
    {
        part_t part;
        part.offsetHead = offset;
        part.offsetData = offset;
        part.size = size;

        QByteArray gmphdr;
        readSubfileHeader<hdr_gmp_t>(file, part, gmphdr);
        const hdr_gmp_t * pGmpHdr = (hdr_gmp_t * )gmphdr.data();

        quint32 offsetTre = gar_load(quint32, pGmpHdr->offsetTRE);
        quint32 offsetRgn = gar_load(quint32, pGmpHdr->offsetRGN);
        quint32 offsetLbl = gar_load(quint32, pGmpHdr->offsetLBL);
        quint32 offsetNet = gar_load(quint32, pGmpHdr->offsetNET);
        quint32 offsetNod = gar_load(quint32, pGmpHdr->offsetNOD);
        quint32 offsetDem = gar_load(quint32, pGmpHdr->offsetDEM);

        auto setOffset = [offset, size, this](quint32 offsetPart, const QString& namePart)
                         {
                             if(offsetPart)
                             {
                                 part_t& part = parts[namePart];
                                 part.offsetHead = offset + offsetPart;
                                 part.offsetData = offset;
                                 part.size = size;
                             }
                         };
        setOffset(offsetTre, "TRE");
        setOffset(offsetRgn, "RGN");
        setOffset(offsetLbl, "LBL");
        setOffset(offsetNet, "NET");
        setOffset(offsetNod, "NOD");
        setOffset(offsetDem, "DEM");
    }
}

void CGarminSubfile::readBasics(CFileExt &file)
{
    if(!parts["TRE"].valid() || !parts["RGN"].valid())
    {
        return;
    }

    const part_t& partTre = parts["TRE"];

    QByteArray trehdr;
    readSubfileHeader<hdr_tre_t>(file, partTre, trehdr);
    const hdr_tre_t * pTreHdr = (hdr_tre_t * )trehdr.data();

    transparent = pTreHdr->POI_flags & 0x02;

#ifdef DEBUG_SHOW_TRE_DATA
    qDebug() << "+++" << subfile.name << "+++";
    qDebug() << "TRE header length  :" << gar_load(uint16_t, pTreHdr->length);
    qDebug() << "TRE1 offset        :" << hex << gar_load(quint32, pTreHdr->tre1_offset);
    qDebug() << "TRE1 size          :" << dec << gar_load(quint32, pTreHdr->tre1_size);
    qDebug() << "TRE2 offset        :" << hex << gar_load(quint32, pTreHdr->tre2_offset);
    qDebug() << "TRE2 size          :" << dec << gar_load(quint32, pTreHdr->tre2_size);
#endif                       // DEBUG_SHOW_TRE_DATA

    copyright = QString(file.data(partTre.offsetHead + gar_load(uint16_t, pTreHdr->size), 0x7FFF));

    // read map boundaries from header
    qint32 i32;
    i32 = gar_ptr_load(int24_t, &pTreHdr->northbound);
    northbound = GARMIN_RAD(i32);
    i32 = gar_ptr_load(int24_t, &pTreHdr->eastbound);
    eastbound = GARMIN_RAD(i32);
    i32 = gar_ptr_load(int24_t, &pTreHdr->southbound);
    southbound = GARMIN_RAD(i32);
    i32 = gar_ptr_load(int24_t, &pTreHdr->westbound);
    westbound = GARMIN_RAD(i32);

    if(eastbound == westbound)
    {
        eastbound = -eastbound;
    }

    if(westbound > 0 && eastbound < 0)
    {
        eastbound = -eastbound;
    }

    area = QRectF(QPointF(westbound, northbound), QPointF(eastbound, southbound));

#ifdef DEBUG_SHOW_TRE_DATA
    qDebug() << "bounding area (\260)" << (northbound * RAD_TO_DEG) << (eastbound * RAD_TO_DEG) << (southbound * RAD_TO_DEG) << (westbound * RAD_TO_DEG);
    qDebug() << "bounding area (rad)" << area;
#endif                       // DEBUG_SHOW_TRE_DATA


    QByteArray maplevel;
    CMapIMG::readFile(file, partTre.offsetData + gar_load(quint32, pTreHdr->tre1_offset), gar_load(quint32, pTreHdr->tre1_size), maplevel);
    const tre_map_level_t * pMapLevel = (const tre_map_level_t * )maplevel.data();

    if(pTreHdr->flag & 0x80)
    {
        throw CMapException(CMapException::errLock, tr("File contains locked / encrypted data. Garmin does not "
                                                       "want you to use this file with any other software than "
                                                       "the one supplied by Garmin."));
    }

    quint32 nlevels       = gar_load(quint32, pTreHdr->tre1_size) / sizeof(tre_map_level_t);
    quint32 nsubdivs      = 0;
    quint32 nsubdivs_last = 0;

    // count subsections
    for(quint32 i = 0; i < nlevels; ++i)
    {
        map_level_t ml;
        ml.inherited    = TRE_MAP_INHER(pMapLevel);
        ml.level        = TRE_MAP_LEVEL(pMapLevel);
        ml.bits         = pMapLevel->bits;
        maplevels << ml;
        nsubdivs       += gar_load(uint16_t, pMapLevel->nsubdiv);
        nsubdivs_last   = gar_load(uint16_t, pMapLevel->nsubdiv);
#ifdef DEBUG_SHOW_MAPLEVEL_DATA
        qDebug() << "level" << TRE_MAP_LEVEL(pMapLevel) << "inherited" << TRE_MAP_INHER(pMapLevel)
                 << "bits" << pMapLevel->bits << "#subdivs" << gar_load(uint16_t, pMapLevel->nsubdiv);
#endif                   // DEBUG_SHOW_MAPLEVEL_DATA
        ++pMapLevel;
    }

    quint32 nsubdivs_next = nsubdivs - nsubdivs_last;

    //////////////////////////////////
    // read subdivision information
    //////////////////////////////////
    // point to first map level definition
    pMapLevel = (const tre_map_level_t * )maplevel.data();
    // number of subdivisions per map level
    quint32 nsubdiv = gar_load(uint16_t, pMapLevel->nsubdiv);

    // point to first 16 byte subdivision definition entry
    QByteArray subdiv_n;
    CMapIMG::readFile(file, partTre.offsetData + gar_load(quint32, pTreHdr->tre2_offset), gar_load(quint32, pTreHdr->tre2_size), subdiv_n);
    tre_subdiv_next_t * pSubDivN = (tre_subdiv_next_t*)subdiv_n.data();

    subdivs.resize(nsubdivs);
    QVector<subdiv_desc_t>::iterator subdiv      = subdivs.begin();
    QVector<subdiv_desc_t>::iterator subdiv_prev = subdivs.end();

    // absolute offset of RGN data
    const part_t& partRgn = parts["RGN"];
    QByteArray rgnhdr;
    readSubfileHeader<hdr_rgn_t>(file, partRgn, rgnhdr);
    hdr_rgn_t * pRgnHdr = (hdr_rgn_t*)rgnhdr.data();
    quint32 rgnoff = gar_load(quint32, pRgnHdr->offset1);

    quint32 rgnOffPolyg2 = gar_load(quint32, pRgnHdr->offset_polyg2);
    quint32 rgnOffPolyl2 = gar_load(quint32, pRgnHdr->offset_polyl2);
    quint32 rgnOffPoint2 = gar_load(quint32, pRgnHdr->offset_point2);

    quint32 rgnLenPolyg2 = gar_load(quint32, pRgnHdr->length_polyg2);
    quint32 rgnLenPolyl2 = gar_load(quint32, pRgnHdr->length_polyl2);
    quint32 rgnLenPoint2 = gar_load(quint32, pRgnHdr->length_point2);

    // parse all 16 byte subdivision entries
    quint32 i;
    for(i = 0; i < nsubdivs_next; ++i, --nsubdiv)
    {
        subdiv->n = i;
        subdiv->next         = gar_load(uint16_t, pSubDivN->next);
        subdiv->terminate    = TRE_SUBDIV_TERM(pSubDivN);
        subdiv->rgn_start    = gar_ptr_load(uint24_t, &pSubDivN->rgn_offset);
        subdiv->rgn_start   += rgnoff;
        // skip if this is the first entry
        if(subdiv_prev != subdivs.end())
        {
            subdiv_prev->rgn_end = subdiv->rgn_start;
        }

        subdiv->hasPoints    = pSubDivN->elements & 0x10;
        subdiv->hasIdxPoints = pSubDivN->elements & 0x20;
        subdiv->hasPolylines = pSubDivN->elements & 0x40;
        subdiv->hasPolygons  = pSubDivN->elements & 0x80;

        // if all subdivisions of this level have been parsed, switch to the next one
        if(nsubdiv == 0)
        {
            ++pMapLevel;
            nsubdiv = gar_load(uint16_t, pMapLevel->nsubdiv);
        }

        subdiv->level = TRE_MAP_LEVEL(pMapLevel);
        subdiv->shift = 24 - pMapLevel->bits;

        qint32 cx = gar_ptr_load(uint24_t, &pSubDivN->center_lng);
        subdiv->iCenterLng = cx;
        qint32 cy = gar_ptr_load(uint24_t, &pSubDivN->center_lat);
        subdiv->iCenterLat = cy;
        qint32 width   = TRE_SUBDIV_WIDTH(pSubDivN) << subdiv->shift;
        qint32 height  = gar_load(uint16_t, pSubDivN->height) << subdiv->shift;

        subdiv->north = GARMIN_RAD(cy + height + 1);
        subdiv->south = GARMIN_RAD(cy - height);
        subdiv->east  = GARMIN_RAD(cx + width + 1);
        subdiv->west  = GARMIN_RAD(cx - width);

        subdiv->area = QRectF(QPointF(subdiv->west, subdiv->north), QPointF(subdiv->east, subdiv->south));

        subdiv->offsetPoints2    = 0;
        subdiv->lengthPoints2    = 0;
        subdiv->offsetPolylines2 = 0;
        subdiv->lengthPolylines2 = 0;
        subdiv->offsetPolygons2  = 0;
        subdiv->lengthPolygons2  = 0;

        subdiv_prev = subdiv;
        ++pSubDivN;
        ++subdiv;
    }

    // switch to last map level
    ++pMapLevel;
    // witch pointer to 14 byte subdivision sections
    tre_subdiv_t* pSubDivL = pSubDivN;
    // parse all 14 byte subdivision entries of last map level
    for(; i < nsubdivs; ++i)
    {
        subdiv->n = i;
        subdiv->next         = 0;
        subdiv->terminate    = TRE_SUBDIV_TERM(pSubDivL);
        subdiv->rgn_start    = gar_ptr_load(uint24_t, &pSubDivL->rgn_offset);
        subdiv->rgn_start   += rgnoff;
        subdiv_prev->rgn_end = subdiv->rgn_start;
        subdiv->hasPoints    = pSubDivL->elements & 0x10;
        subdiv->hasIdxPoints = pSubDivL->elements & 0x20;
        subdiv->hasPolylines = pSubDivL->elements & 0x40;
        subdiv->hasPolygons  = pSubDivL->elements & 0x80;

        subdiv->level = TRE_MAP_LEVEL(pMapLevel);
        subdiv->shift = 24 - pMapLevel->bits;

        qint32 cx = gar_ptr_load(uint24_t, &pSubDivL->center_lng);
        subdiv->iCenterLng = cx;
        qint32 cy = gar_ptr_load(uint24_t, &pSubDivL->center_lat);
        subdiv->iCenterLat = cy;
        qint32 width   = TRE_SUBDIV_WIDTH(pSubDivL) << subdiv->shift;
        qint32 height  = gar_load(uint16_t, pSubDivL->height) << subdiv->shift;

        subdiv->north = GARMIN_RAD(cy + height + 1);
        subdiv->south = GARMIN_RAD(cy - height);
        subdiv->east  = GARMIN_RAD(cx + width + 1);
        subdiv->west  = GARMIN_RAD(cx - width);

        subdiv->area = QRectF(QPointF(subdiv->west, subdiv->north), QPointF(subdiv->east, subdiv->south));

        subdiv->offsetPoints2    = 0;
        subdiv->lengthPoints2    = 0;
        subdiv->offsetPolylines2 = 0;
        subdiv->lengthPolylines2 = 0;
        subdiv->offsetPolygons2  = 0;
        subdiv->lengthPolygons2  = 0;

        subdiv_prev = subdiv;
        ++pSubDivL;
        ++subdiv;
    }
    subdivs.last().rgn_end = gar_load(quint32, pRgnHdr->offset1) + gar_load(quint32, pRgnHdr->size);

    // read extended NT elements
    if((gar_load(uint16_t, pTreHdr->hdr_subfile_part_t::size) >= 0x9A) && pTreHdr->tre7_size)
    {
        const quint32 recSize = gar_load(quint32, pTreHdr->tre7_rec_size);
        QByteArray subdiv2;
        CMapIMG::readFile(file, partTre.offsetData + gar_load(quint32, pTreHdr->tre7_offset), gar_load(quint32, pTreHdr->tre7_size), subdiv2);
        quint32 * pSubDiv2 = (quint32*)subdiv2.data();

        subdiv       = subdivs.begin();
        subdiv_prev  = subdivs.begin();

        subdiv->offsetPolygons2 = (recSize >= 4) ? gar_load(quint32, pSubDiv2[0]) + rgnOffPolyg2 : rgnOffPolyg2;
        subdiv->offsetPolylines2 = (recSize >= 8) ? gar_load(quint32, pSubDiv2[1]) + rgnOffPolyl2 : rgnOffPolyl2;
        subdiv->offsetPoints2 = (recSize >= 12) ? gar_load(quint32, pSubDiv2[2]) + rgnOffPoint2 : rgnOffPoint2;

        ++subdiv;
        pSubDiv2 = reinterpret_cast<quint32*>((quint8*)pSubDiv2 + recSize);

        while(subdiv != subdivs.end())
        {
            subdiv->offsetPolygons2 = (recSize >= 4) ? gar_load(quint32, pSubDiv2[0]) + rgnOffPolyg2 : rgnOffPolyg2;
            subdiv->offsetPolylines2 = (recSize >= 8) ? gar_load(quint32, pSubDiv2[1]) + rgnOffPolyl2 : rgnOffPolyl2;
            subdiv->offsetPoints2 = (recSize >= 12) ? gar_load(quint32, pSubDiv2[2]) + rgnOffPoint2 : rgnOffPoint2;

            subdiv_prev->lengthPolygons2 = subdiv->offsetPolygons2 - subdiv_prev->offsetPolygons2;
            subdiv_prev->lengthPolylines2 = subdiv->offsetPolylines2 - subdiv_prev->offsetPolylines2;
            subdiv_prev->lengthPoints2 = subdiv->offsetPoints2 - subdiv_prev->offsetPoints2;

            subdiv_prev = subdiv;

            ++subdiv;
            pSubDiv2 = reinterpret_cast<quint32*>((quint8*)pSubDiv2 + recSize);
        }

        subdiv_prev->lengthPolygons2  = rgnOffPolyg2 + rgnLenPolyg2 - subdiv_prev->offsetPolygons2;
        subdiv_prev->lengthPolylines2 = rgnOffPolyl2 + rgnLenPolyl2 - subdiv_prev->offsetPolylines2;
        subdiv_prev->lengthPoints2    = rgnOffPoint2 + rgnLenPoint2 - subdiv_prev->offsetPoints2;
    }

#ifdef DEBUG_SHOW_SUBDIV_DATA
    {
        QVector<subdiv_desc_t>::iterator subdiv = subfile.subdivs.begin();
        while(subdiv != subfile.subdivs.end())
        {
            qDebug() << "--- subdiv" << subdiv->n << "---";
            qDebug() << "RGN start          " << hex << subdiv->rgn_start;
            qDebug() << "RGN end            " << hex << subdiv->rgn_end;
            qDebug() << "center lng         " << GARMIN_DEG(subdiv->iCenterLng);
            qDebug() << "center lat         " << GARMIN_DEG(subdiv->iCenterLat);
            qDebug() << "has points         " << subdiv->hasPoints;
            qDebug() << "has indexed points " << subdiv->hasIdxPoints;
            qDebug() << "has polylines      " << subdiv->hasPolylines;
            qDebug() << "has polygons       " << subdiv->hasPolygons;
            qDebug() << "bounding area (m)  " << subdiv->area.topLeft() << subdiv->area.bottomRight();
            qDebug() << "map level          " << subdiv->level;
            qDebug() << "left shifts        " << subdiv->shift;

            qDebug() << "polyg off.         " << hex << subdiv->offsetPolygons2;
            qDebug() << "polyg len.         " << hex << subdiv->lengthPolygons2;
            qDebug() << "polyl off.         " << hex << subdiv->offsetPolylines2;
            qDebug() << "polyl len.         " << hex << subdiv->lengthPolylines2;
            qDebug() << "point off.         " << hex << subdiv->offsetPoints2;
            qDebug() << "point len.         " << hex << subdiv->lengthPoints2;
            ++subdiv;
        }
    }
#endif                       // DEBUG_SHOW_SUBDIV_DATA

    const part_t& partLbl = parts["LBL"];
    const part_t& partNet = parts["NET"];

    if(partLbl.valid())
    {
        QByteArray lblhdr;
        readSubfileHeader<hdr_lbl_t>(file, partLbl, lblhdr);
        hdr_lbl_t * pLblHdr = (hdr_lbl_t*)lblhdr.data();

        quint32 offsetLbl1 = partLbl.offsetData + gar_load(quint32, pLblHdr->lbl1_offset);
        quint32 offsetLbl6 = partLbl.offsetData + gar_load(quint32, pLblHdr->lbl6_offset);

        QByteArray nethdr;
        quint32 offsetNet1  = 0;
        hdr_net_t * pNetHdr = nullptr;
        if(partNet.valid())
        {
            readSubfileHeader<hdr_net_t>(file, partNet, nethdr);
            pNetHdr = (hdr_net_t*)nethdr.data();
            offsetNet1 = partNet.offsetData + gar_load(quint32, pNetHdr->net1_offset);
        }

        quint16 codepage = 0;
        if(gar_load(uint16_t, pLblHdr->size) > 0xAA)
        {
            codepage = gar_load(uint16_t, pLblHdr->codepage);
        }

        //         qDebug() << file.fileName() << hex << offsetLbl1 << offsetLbl6 << offsetNet1;

        switch(pLblHdr->coding)
        {
        case 0x06:
            strtbl = new CGarminStrTbl6(codepage, nullptr);
            break;

        case 0x09:
            strtbl = new CGarminStrTbl8(codepage, nullptr);
            break;

        case 0x0A:
            strtbl = new CGarminStrTblUtf8(codepage, nullptr);
            break;

        default:
            qWarning() << "Unknown label coding" << hex << pLblHdr->coding;
        }

        if(!strtbl.isNull())
        {
            strtbl->registerLBL1(offsetLbl1, gar_load(quint32, pLblHdr->lbl1_length), pLblHdr->addr_shift);
            strtbl->registerLBL6(offsetLbl6, gar_load(quint32, pLblHdr->lbl6_length));
            if(nullptr != pNetHdr)
            {
                strtbl->registerNET1(offsetNet1, gar_load(quint32, pNetHdr->net1_length), pNetHdr->net1_addr_shift);
            }
        }
    }
}
