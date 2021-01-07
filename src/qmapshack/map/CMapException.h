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

#ifndef CMAPEXCEPTION_H
#define CMAPEXCEPTION_H

#include <QException>

class CMapException : public QException
{
public:
    enum exce_e {eErrOpen, eErrAccess, errFormat, errLock, errAbort};

    CMapException(exce_e err, const QString& msg);
    virtual ~CMapException() = default;

    QString msg;
    exce_e err;
};

#endif //CMAPEXCEPTION_H

