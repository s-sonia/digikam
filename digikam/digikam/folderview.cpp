/* ============================================================
 * File  : folderview.cpp
 * Author: Joern Ahrens <joern.ahrens@kdemail.net>
 * Date  : 2005-05-06
 * Copyright 2005 by Joern Ahrens
 *
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
 * ============================================================ */

#include <kglobalsettings.h>
#include <kcursor.h>
#include <kdebug.h>

#include "folderview.h"

//-----------------------------------------------------------------------------
// FolderViewPriv
//-----------------------------------------------------------------------------

class FolderViewPriv
{
public:
    bool        active;
};

//-----------------------------------------------------------------------------
// FolderViewPriv
//-----------------------------------------------------------------------------

FolderView::FolderView(QWidget *parent)
    : QListView(parent)
{
    d = new FolderViewPriv;
    
    d->active = false;
}

FolderView::~FolderView()
{
    delete d;
}

void FolderView::setActive(bool val)
{
    d->active = val;

    if (d->active)
        slotSelectionChanged();
}

bool FolderView::active()
{
    return d->active;
}

void FolderView::contentsMouseMoveEvent(QMouseEvent *e)
{
    QListView::contentsMouseMoveEvent(e);

    if(e->state() == NoButton)
    {
        if(KGlobalSettings::changeCursorOverIcon())
        {
            QPoint vp = contentsToViewport(e->pos());
            QListViewItem *item = itemAt(vp);
            if (mouseInItemRect(item, vp.x()))
                setCursor(KCursor::handCursor());
            else
                unsetCursor();
        }
        return;
    }
}

bool FolderView::mouseInItemRect(QListViewItem* item, int x) const
{
    if (!item)
        return false;
    
    x += contentsX();

    int offset = treeStepSize()*(item->depth() + (rootIsDecorated() ? 1 : 0));
    offset    += itemMargin();
    int width = item->width(fontMetrics(), this, 0);
    
    return (x > offset && x < (offset + width));
}


#include "folderview.moc"
