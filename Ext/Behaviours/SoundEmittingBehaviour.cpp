/*
	This file is part of the MinSG library extension Behaviours.
	Copyright (C) 2011 Sascha Brandt
	Copyright (C) 2009-2012 Benjamin Eikel <benjamin@eikel.org>
	Copyright (C) 2009-2012 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2009-2012 Ralf Petring <ralf@petring.net>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifdef MINSG_EXT_SOUND

#include "SoundEmittingBehaviour.h"
#include "../../Core/Nodes/Node.h"

namespace MinSG {

//! (ctor)
SoundEmittingBehaviour::SoundEmittingBehaviour(Node * node) :
		AbstractNodeBehaviour(node),
		source(Sound::Source::create()),removeWhenStopped(true),lastPosition(node->getWorldOrigin()),lastTime(0) {
	//ctor
}

//! (dtor)
SoundEmittingBehaviour::~SoundEmittingBehaviour() {
	if(source && source->isPlaying())
		source->pause();
	//dtor
}

//!	---|> AbstractBehaviour
AbstractBehaviour::behaviourResult_t SoundEmittingBehaviour::doExecute() {
	if(!source || (source->isStopped() && removeWhenStopped)) {
//		std::cout << "SoundEmittingBehaviour removed.\n";
		return FINISHED;
	}
	const timestamp_t timeSec = getCurrentTime();
	if(lastTime==0)
		lastTime=timeSec;
	const timestamp_t tDiff = timeSec-lastTime;

	using namespace Geometry;
	const Vec3 newPos( getNode()->getWorldOrigin() );

	// update velocity
	Vec3 velocity( tDiff>0 ? (newPos-lastPosition)*(1.0/tDiff) : Vec3(0,0,0));
	float oldX,oldY,oldZ;
	source->getVelocity(oldX,oldY,oldZ);
	velocity = (velocity+Vec3(oldX,oldY,oldZ)) * 0.5;
	source->setVelocity(velocity.x(),velocity.y(),velocity.z());

	// update position
	source->setPosition(newPos.x(),newPos.y(),newPos.z());

	//! \todo Update direction!!!

	lastTime = timeSec;
	lastPosition = newPos;

	return CONTINUE;
}
//!	---|> AbstractBehaviour
void SoundEmittingBehaviour::onInit(){
	if(!source->isPlaying())
		source->play();
//	std::cout << "play";
}


void SoundEmittingBehaviour::doFinalize(BehaviorStatus &){
	if(source){
		source->stop();
		source = nullptr;
	}
}

}

#endif // MINSG_EXT_SOUND
