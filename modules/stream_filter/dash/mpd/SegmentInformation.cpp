/*
 * SegmentInformation.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "SegmentInformation.hpp"

#include "Segment.h"
#include "SegmentBase.h"
#include "SegmentList.h"
#include "SegmentTemplate.h"

using namespace dash::mpd;
using namespace std;

SegmentInformation::SegmentInformation(SegmentInformation *parent_) :
    ICanonicalUrl( parent_ )
{
    parent = parent_;
    segmentBase = NULL;
    segmentList = NULL;
    for(int i=0; i<InfoTypeCount; i++)
        segmentTemplate[i] = NULL;
}

SegmentInformation::SegmentInformation(ICanonicalUrl * parent_) :
    ICanonicalUrl( parent_ )
{
    parent = NULL;
    segmentBase = NULL;
    segmentList = NULL;
    for(int i=0; i<InfoTypeCount; i++)
        segmentTemplate[i] = NULL;
}

SegmentInformation::~SegmentInformation()
{
    delete segmentBase;
    delete segmentList;
    for(int i=0; i<InfoTypeCount; i++)
        delete segmentTemplate[i];
}

vector<ISegment *> SegmentInformation::getSegments() const
{
    vector<ISegment *> retSegments;

    SegmentBase *segBase = inheritSegmentBase();
    SegmentList *segList = inheritSegmentList();

    /* init segments are always single segment */
    if( segBase && segBase->getInitSegment() )
    {
        retSegments.push_back( segBase->getInitSegment() );
    }
    else if ( segList && segList->getInitialisationSegment() )
    {
        retSegments.push_back( segList->getInitialisationSegment() );
    }
    else if( inheritSegmentTemplate(INFOTYPE_INIT) )
    {
        retSegments.push_back( inheritSegmentTemplate(INFOTYPE_INIT) );
    }

    if( inheritSegmentTemplate(INFOTYPE_MEDIA) )
    {
        retSegments.push_back( inheritSegmentTemplate(INFOTYPE_MEDIA) );
    }
    else if ( segList && !segList->getSegments().empty() )
    {
        std::vector<Segment *>::const_iterator it;
        for(it=segList->getSegments().begin();
            it!=segList->getSegments().end(); it++)
        {
            std::vector<ISegment *> list = (*it)->subSegments();
            retSegments.insert( retSegments.end(), list.begin(), list.end() );
        }
    }

    return retSegments;
}

void SegmentInformation::setSegmentList(SegmentList *list)
{
    segmentList = list;
}

void SegmentInformation::setSegmentBase(SegmentBase *base)
{
    segmentBase = base;
}

void SegmentInformation::setSegmentTemplate(SegmentTemplate *templ, SegmentInfoType type)
{
    segmentTemplate[type] = templ;
}

static void insertIntoSegment(std::vector<Segment *> &seglist, size_t start,
                              size_t end, mtime_t time)
{
    std::vector<Segment *>::iterator segIt;
    for(segIt = seglist.begin(); segIt < seglist.end(); segIt++)
    {
        Segment *segment = *segIt;
        if(segment->getClassId() == Segment::CLASSID_SEGMENT &&
           segment->contains(end + segment->getOffset()))
        {
            SubSegment *subsegment = new SubSegment(segment,
                                                    start + segment->getOffset(),
                                                    end + segment->getOffset());
            segment->addSubSegment(subsegment);
            segment->setStartTime(time);
            break;
        }
    }
}

void SegmentInformation::SplitUsingIndex(std::vector<SplitPoint> &splitlist)
{
    std::vector<Segment *> seglist = segmentList->getSegments();
    std::vector<SplitPoint>::const_iterator splitIt;
    size_t start = 0, end = 0;
    mtime_t time = 0;

    for(splitIt = splitlist.begin(); splitIt < splitlist.end(); splitIt++)
    {
        start = end;
        SplitPoint split = *splitIt;
        end = split.offset;
        if(splitIt == splitlist.begin() && split.offset == 0)
            continue;
        time = split.time;
        insertIntoSegment(seglist, start, end, time);
        end++;
    }

    if(start != 0)
    {
        start = end;
        end = 0;
        insertIntoSegment(seglist, start, end, time);
    }
}

SegmentBase * SegmentInformation::inheritSegmentBase() const
{
    if(segmentBase)
        return segmentBase;
    else if (parent)
        return parent->inheritSegmentBase();
    else
        return NULL;
}

SegmentList * SegmentInformation::inheritSegmentList() const
{
    if(segmentList)
        return segmentList;
    else if (parent)
        return parent->inheritSegmentList();
    else
        return NULL;
}

SegmentTemplate * SegmentInformation::inheritSegmentTemplate(SegmentInfoType type) const
{
    if(segmentTemplate[type])
        return segmentTemplate[type];
    else if (parent)
        return parent->inheritSegmentTemplate(type);
    else
        return NULL;
}
