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

#ifndef CDATASTREAMV1_H
#define CDATASTREAMV1_H

#include <QDataStream>
#include <QFloat16>
#include <QString>

class CDataStreamV1 : public QDataStream
{
public:
    CDataStreamV1(const QByteArray &a);
    CDataStreamV1() = delete;
    CDataStreamV1(QIODevice *d);
    CDataStreamV1(QByteArray *a, QIODevice::OpenMode mode);

    virtual ~CDataStreamV1() = default;


    CDataStreamV1 & operator<<(qint8 i)
    {
        QDataStream::operator<<(i);
        return *this;
    }
    CDataStreamV1 & operator<<(quint8 i)
    {
        QDataStream::operator<<(i);
        return *this;
    }
    CDataStreamV1 & operator<<(qint16 i)
    {
        QDataStream::operator<<(i);
        return *this;
    }
    CDataStreamV1 & operator<<(quint16 i)
    {
        QDataStream::operator<<(i);
        return *this;
    }
    CDataStreamV1 & operator<<(qint32 i)
    {
        QDataStream::operator<<(i);
        return *this;
    }
    CDataStreamV1 & operator<<(quint32 i)
    {
        QDataStream::operator <<(i);
        return *this;
    }
    CDataStreamV1 & operator<<(qint64 i)
    {
        QDataStream::operator<<(i);
        return *this;
    }
    CDataStreamV1 & operator<<(quint64 i)
    {
        QDataStream::operator<<(i);
        return *this;
    }
    CDataStreamV1 & operator<<(bool i)
    {
        QDataStream::operator<<(i);
        return *this;
    }
    CDataStreamV1 & operator<<(qfloat16 f)
    {
        QDataStream::operator<<(f);
        return *this;
    }
    CDataStreamV1 & operator<<(float f)
    {
        QDataStream::operator<<(f);
        return *this;
    }
    CDataStreamV1 & operator<<(double f)
    {
        QDataStream::operator<<(f);
        return *this;
    }
    CDataStreamV1 & operator<<(const char *s)
    {
        QDataStream::operator<<(s);
        return *this;
    }

    CDataStreamV1 & operator>>(qint8 &i)
    {
        QDataStream::operator>>(i);
        return *this;
    }
    CDataStreamV1 & operator>>(quint8 &i)
    {
        QDataStream::operator>>(i);
        return *this;
    }
    CDataStreamV1 & operator>>(qint16 &i)
    {
        QDataStream::operator>>(i);
        return *this;
    }
    CDataStreamV1 & operator>>(quint16 &i)
    {
        QDataStream::operator>>(i);
        return *this;
    }
    CDataStreamV1 & operator>>(qint32 &i)
    {
        QDataStream::operator>>(i);
        return *this;
    }
    CDataStreamV1 & operator>>(quint32 &i)
    {
        QDataStream::operator>>(i);
        return *this;
    }
    CDataStreamV1 & operator>>(qint64 &i)
    {
        QDataStream::operator>>(i);
        return *this;
    }
    CDataStreamV1 & operator>>(quint64 &i)
    {
        QDataStream::operator>>(i);
        return *this;
    }
    CDataStreamV1 & operator>>(bool &i)
    {
        QDataStream::operator>>(i);
        return *this;
    }
    CDataStreamV1 & operator>>(qfloat16 &f)
    {
        QDataStream::operator>>(f);
        return *this;
    }
    CDataStreamV1 & operator>>(float &f)
    {
        QDataStream::operator>>(f);
        return *this;
    }
    CDataStreamV1 & operator>>(double &f)
    {
        QDataStream::operator>>(f);
        return *this;
    }
    CDataStreamV1 & operator>>(char *&s)
    {
        QDataStream::operator>>(s);
        return *this;
    }
};

inline CDataStreamV1 & operator<<(CDataStreamV1 &stream, const QString &string)
{
    dynamic_cast<QDataStream&>(stream) << string;
    return stream;
}

inline CDataStreamV1 & operator>>(CDataStreamV1 &stream, QString &string)
{
    dynamic_cast<QDataStream&>(stream) >> string;
    return stream;
}

inline CDataStreamV1 & operator<<(CDataStreamV1 &stream, const QByteArray &bytearray)
{
    dynamic_cast<QDataStream&>(stream) << bytearray;
    return stream;
}

inline CDataStreamV1 & operator>>(CDataStreamV1 &stream, QByteArray &bytearray)
{
    dynamic_cast<QDataStream&>(stream) >> bytearray;
    return stream;
}

#endif //CDATASTREAMV1_H

