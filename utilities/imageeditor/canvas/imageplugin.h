/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2004-06-04
 * Description : image plugins interface for image editor.
 * 
 * Copyright (C) 2004-2005 by Renchi Raju <renchi@pooh.tam.uiuc.edu>

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * ============================================================ */

#ifndef IMAGEPLUGIN_H
#define IMAGEPLUGIN_H

// Qt includes.

#include <QObject>

// KDE includes.

#include <kxmlguiclient.h>

// Local includes.

#include "digikam_export.h"

class QWidget;

namespace Digikam
{

class DIGIKAM_EXPORT ImagePlugin : public QObject, public KXMLGUIClient
{
public:
    
    ImagePlugin(QObject *parent, const char* name=0);
    virtual ~ImagePlugin();

    virtual void setEnabledSelectionActions(bool enable);
    virtual void setEnabledActions(bool enable);

    void     setParentWidget(QWidget* parent);
    
    QWidget *parentWidget();
    
private:

    QWidget *m_parentWidget;
};

}  //namespace Digikam

#endif /* IMAGEPLUGIN_H */

