/****************************************************************************
**
 * Draw circle by foci and a point on circle

Copyright (C) 2012 Dongxu Li (dongxuli2011@gmail.com)
Copyright (C) 2011 R. van Twisk (librecad@rvt.dds.nl)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**********************************************************************/

#include <QAction>
#include <QMouseEvent>
#include "rs_actiondrawcircletan2_1p.h"

#include "rs_dialogfactory.h"
#include "rs_graphicview.h"
#include "rs_commandevent.h"
#include "rs_arc.h"
#include "rs_circle.h"
#include "lc_quadratic.h"
#include "rs_coordinateevent.h"

namespace {
auto enTypeList={RS2::EntityLine, RS2::EntityArc, RS2::EntityCircle};
}
/**
 * Constructor.
 *
 */
RS_ActionDrawCircleTan2_1P::RS_ActionDrawCircleTan2_1P(
        RS_EntityContainer& container,
        RS_GraphicView& graphicView)
    :RS_PreviewActionInterface("Draw tangent circle 2P",
                               container, graphicView),
	  cData(new RS_CircleData(RS_Vector(0.,0.),1.))
	,radius(0.)
	,valid(false)
{
	actionType=RS2::ActionDrawCircleTan2_1P;
}

RS_ActionDrawCircleTan2_1P::~RS_ActionDrawCircleTan2_1P() = default;

void RS_ActionDrawCircleTan2_1P::init(int status) {
    if(status>=0) {
        RS_Snapper::suspend();
    }
	if(status>(int) circles.size()) status=(int) circles.size();
    RS_PreviewActionInterface::init(status);
	for(int i=0; i<status; ++i){
		if(!circles[i]) {
            status=i;
            break;
        }
    }
    bool updateNeeded(false);
	for(size_t i=status>=0?status:0; i<circles.size(); ++i){
        if(circles[i])
            if(circles[i]->isHighlighted()){
                circles[i]->setHighlighted(false);
                updateNeeded=true;
            }
    }
    if(updateNeeded) graphicView->redraw(RS2::RedrawDrawing);
	circles.resize(status>=0?status:0);
}


void RS_ActionDrawCircleTan2_1P::finish(bool updateTB){
    if( circles.size() >0) {
		for(RS_AtomicEntity*const circle: circles)
            circle->setHighlighted(false);
        graphicView->redraw(RS2::RedrawDrawing);
    }
    RS_PreviewActionInterface::finish(updateTB);
}

void RS_ActionDrawCircleTan2_1P::trigger() {
    RS_PreviewActionInterface::trigger();
	RS_Circle* c=new RS_Circle(container, *cData);

    container->addEntity(c);

    // upd. undo list:
    if (document) {
        document->startUndoCycle();
        document->addUndoable(c);
        document->endUndoCycle();
    }


	for(RS_AtomicEntity*const circle: circles)
        circle->setHighlighted(false);
    graphicView->redraw(RS2::RedrawDrawing);
    circles.clear();


    RS_DEBUG->print("RS_ActionDrawCircleTan2_1P::trigger():"
                    " entity added: %d", c->getId());
    init(SetCircle1);
}


bool RS_ActionDrawCircleTan2_1P::getCenters()
{
    if(circles.size()<2) return false;
    LC_Quadratic lc0(circles[0], point);
    LC_Quadratic lc1(circles[1], point);

	auto list=LC_Quadratic::getIntersection(lc0,lc1);
    centers.clean();
	for(const RS_Vector& vp: list){
		auto ds=vp.distanceTo(point)-RS_TOLERANCE;
        bool validBranch(true);
        for(int j=0;j<2;j++){
            if(circles[j]->rtti()==RS2::EntityCircle||circles[j]->rtti()==RS2::EntityArc){
                if( vp.distanceTo(circles[j]->getCenter()) <= ds) {
                    validBranch=false;
                    break;
                }
            }
        }
        if(validBranch)  centers.push_back(vp);
    }
    return centers.size()>0;
}

void RS_ActionDrawCircleTan2_1P::mouseMoveEvent(QMouseEvent* e) {
    RS_DEBUG->print("RS_ActionDrawCircleTan2_1P::mouseMoveEvent begin");

    switch( getStatus()){
    case SetPoint:
        coord=snapPoint(e);
        point=coord;
        break;
    case SetCenter:
        coord=graphicView->toGraph(e->x(),e->y());
        break;
    default:
        return;
    }
    deletePreview();
    if(preparePreview()){
		RS_Circle* e=new RS_Circle(preview.get(), *cData);
        preview->addEntity(e);
        drawPreview();
    }
    RS_DEBUG->print("RS_ActionDrawCircleTan2_1P::mouseMoveEvent end");
}

bool RS_ActionDrawCircleTan2_1P::preparePreview(){
	if (!getCenters()) return false;
	cData->center=centers.getClosest(coord);
	cData->radius=point.distanceTo(cData->center);
    return true;
}

RS_Entity* RS_ActionDrawCircleTan2_1P::catchCircle(QMouseEvent* e) {
	RS_Entity* ret=nullptr;
    RS_Entity*  en = catchEntity(e,enTypeList, RS2::ResolveAll);
	if (!en) return ret;
	if (!en->isVisible()) return ret;
	for(auto p: circles){
		if(p && en->getId() == p->getId()) return ret; //do not pull in the same line again
    }
	if(en->getParent() && en->getParent()->ignoredOnModification()){
		return nullptr;
	}
	return en;
}

void RS_ActionDrawCircleTan2_1P::mouseReleaseEvent(QMouseEvent* e) {
    // Proceed to next status
    if (e->button()==Qt::LeftButton) {

        switch (getStatus()) {
        case SetCircle1:
        case SetCircle2:
        {
            circles.resize(getStatus());
            RS_AtomicEntity*  en = static_cast<RS_AtomicEntity*>(catchCircle(e));
			if (!en) return;
//            circle = static_cast<RS_AtomicEntity*>(en);
            en->setHighlighted(true);
			circles.push_back(en);
            graphicView->redraw(RS2::RedrawDrawing);
            setStatus(getStatus()+1);
        }
            break;
        case SetPoint:
        {
            RS_Vector snapped = snapPoint(e);
            RS_CoordinateEvent ce(snapped);
            coordinateEvent(&ce);
        }
            break;
        case SetCenter:
            coord=graphicView->toGraph(e->x(),e->y());
            if(preparePreview()) trigger();
            break;

        default:
            break;
        }
    } else if (e->button()==Qt::RightButton) {
        // Return to last status:
        if(getStatus()>0){
            deletePreview();
        }
        init(getStatus()-1);
    }
}


void RS_ActionDrawCircleTan2_1P::coordinateEvent(RS_CoordinateEvent* e) {

    RS_Vector mouse = e->getCoordinate();
    switch(getStatus()){

    case SetPoint:
        point=mouse;
        coord=mouse;
        if(getCenters()) {
            if(centers.size()==1) trigger();
            else setStatus(getStatus()+1);
        }
        break;
        default:
        break;
//    case SetCenter:
//        coord=mouse;
//        trigger();
    }

}

//fixme, support command line

/*
void RS_ActionDrawCircleTan2_1P::commandEvent(RS_CommandEvent* e) {
    QString c = e->getCommand().toLower();

    if (checkCommand("help", c)) {
        if (RS_DIALOGFACTORY) {
            RS_DIALOGFACTORY->commandMessage(msgAvailableCommands()
                                             + getAvailableCommands().join(", "));
        }
        return;
    }

    switch (getStatus()) {
    case SetFocus1: {
            bool ok;
            double m = RS_Math::eval(c, &ok);
			if (ok) {
                ratio = m / major.magnitude();
                if (!isArc) {
                    trigger();
                } else {
                    setStatus(SetAngle1);
                }
            } else {
                if (RS_DIALOGFACTORY) {
                    RS_DIALOGFACTORY->commandMessage(tr("Not a valid expression"));
                }
            }
        }
        break;

    case SetAngle1: {
            bool ok;
            double a = RS_Math::eval(c, &ok);
			if (ok) {
                angle1 = RS_Math::deg2rad(a);
                setStatus(SetAngle2);
            } else {
                if (RS_DIALOGFACTORY) {
                    RS_DIALOGFACTORY->commandMessage(tr("Not a valid expression"));
                }
            }
        }
        break;

    case SetAngle2: {
            bool ok;
            double a = RS_Math::eval(c, &ok);
			if (ok) {
                angle2 = RS_Math::deg2rad(a);
                trigger();
            } else {
                if (RS_DIALOGFACTORY) {
                    RS_DIALOGFACTORY->commandMessage(tr("Not a valid expression"));
                }
            }
        }
        break;

    default:
        break;
    }
}
*/

QStringList RS_ActionDrawCircleTan2_1P::getAvailableCommands() {
    QStringList cmd;
    return cmd;
}

void RS_ActionDrawCircleTan2_1P::updateMouseButtonHints() {
    if (RS_DIALOGFACTORY) {
        switch (getStatus()) {
        case SetCircle1:
            RS_DIALOGFACTORY->updateMouseWidget(tr("Specify a line/arc/circle"),
                                                tr("Cancel"));
            break;

        case SetCircle2:
            RS_DIALOGFACTORY->updateMouseWidget(tr("Specify the another arc/circle"),
                                                tr("Back"));
            break;

        case SetPoint:
            RS_DIALOGFACTORY->updateMouseWidget(tr("Specify a point on the tangent circle"),
                                                tr("Back"));
            break;
        case SetCenter:
            RS_DIALOGFACTORY->updateMouseWidget(tr("Select the center of the tangent circle"),
                                                tr("Back"));
            break;
        default:
            RS_DIALOGFACTORY->updateMouseWidget();
            break;
        }
    }
}

void RS_ActionDrawCircleTan2_1P::updateMouseCursor() {
    graphicView->setMouseCursor(RS2::CadCursor);
}

// EOF
