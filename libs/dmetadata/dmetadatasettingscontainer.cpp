/* ============================================================
 *
 * This file is a part of digiKam project
 * http://www.digikam.org
 *
 * Date        : 2015-06-22
 * Description : Metadata Settings Container.
 *
 * Copyright (C) 2010-2015 by Gilles Caulier <caulier dot gilles at gmail dot com>
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
 *
 * ============================================================ */

#include "metadatasettingscontainer.h"

// KDE includes

#include <kconfiggroup.h>

// Local includes

#include "dmetadatasettings.h"

namespace Digikam
{

DMetadataSettingsContainer::DMetadataSettingsContainer()
{

}

void DMetadataSettingsContainer::readFromConfig(KConfigGroup& group)
{
    if(group.hasGroup("namespaces"))
    {
        KConfigGroup myItems = group.group("namespaces");

        for(QString element : myItems.groupList())
        {
            KConfigGroup gr = myItems.group(element);
            NamespaceEntry ns(
                        element,
                        (NamespaceEntry::Type)gr.readEntry("Type").toInt(),
                        gr.readEntry("separator"),
                        gr.readEntry("extraXml"));

           namespaceEntries.append(ns);
        }

    }
    else
    {
        defaultValues();
    }
}

void DMetadataSettingsContainer::writeToConfig(KConfigGroup& group) const
{
    KConfigGroup namespacesGroup = group.group("namespaces");

    for(NamespaceEntry e : namespaceEntries)
    {
        KConfigGroup tmp = namespacesGroup.group(e.namespaceName);
        tmp.writeEntry("Type",(int)e.tagPaths);
        tmp.writeEntry("separator", e.separator);
        tmp.writeEntry("extraXml",e.extraXml);
    }

    group.sync();
}

void DMetadataSettingsContainer::defaultValues()
{
    this->namespaceEntries.clear();

    namespaceEntries.append(NamespaceEntry(QLatin1String("Xmp.digiKam.TagsList"),
                                           NamespaceEntry::TAGPATH,
                                           QLatin1String("/"),
                                           QString()));
    namespaceEntries.append(NamespaceEntry(QLatin1String("Xmp.MicrosoftPhoto.LastKeywordXMP"),
                                           NamespaceEntry::TAGPATH,
                                           QLatin1String("/"),
                                           QString()));
    namespaceEntries.append(NamespaceEntry(QLatin1String("Xmp.lr.hierarchicalSubject"),
                                           NamespaceEntry::TAGPATH,
                                           QLatin1String("|"),
                                           QString()));
    namespaceEntries.append(NamespaceEntry(QLatin1String("Xmp.mediapro.CatalogSets"),
                                           NamespaceEntry::TAGPATH,
                                           QLatin1String("|"),
                                           QString()));
}

}  // namespace Digikam