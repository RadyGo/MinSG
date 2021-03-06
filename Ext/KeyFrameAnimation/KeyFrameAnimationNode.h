/*
	This file is part of the MinSG library extension KeyFrameAnimation.
	Copyright (C) 2010-2012 Benjamin Eikel <benjamin@eikel.org>
	Copyright (C) 2010-2012 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2010 David Maicher
	Copyright (C) 2010-2012 Ralf Petring <ralf@petring.net>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef KEYFRAMEANIMATIONNODE_H_
#define KEYFRAMEANIMATIONNODE_H_

#include "../../Core/Nodes/GeometryNode.h"
#include "KeyFrameAnimationData.h"
#include <Rendering/Mesh/Mesh.h>
#include <Rendering/Mesh/MeshVertexData.h>
#include <Rendering/Mesh/VertexDescription.h>

namespace MinSG {

class KeyFrameAnimationBehaviour;

//! @ingroup ext
class KeyFrameAnimationNode:public GeometryNode{
	PROVIDES_TYPE_NAME(KeyFrameAnimationNode)

	public:
		static const short STATE_LOOP_MODE;
		static const short STATE_SINGLE_MODE;
		static const short STATE_SINGLE_MODE_FINISHED;


		KeyFrameAnimationNode(const Rendering::MeshIndexData & indexData, const std::vector<Rendering::MeshVertexData> & framesData,
				std::map<std::string, std::vector<int> > animationData);
		KeyFrameAnimationNode(const KeyFrameAnimationNode & source, Rendering::VertexDescription _vertexDescription,
								std::pair<std::string, std::vector<int> > _activeAnimation,
				float _curAnimationPosition, float _curTime, float _speedFactor, float _lastTimeStamp, short _curState);
		~KeyFrameAnimationNode();

		Rendering::Mesh * createInitialMesh() const;

		bool updateMesh(float timeStampSec);

		void setVertexData(Rendering::MeshVertexData & vertexData, int startFrameIndex, int endFrameIndex, float interpolatePercentage) const;
		bool setActiveAnimation(const std::string & name);

		std::map<std::string, std::vector<int> > getAnimationData();

		std::string getActiveAnimationName(){
			return activeAnimation.first;
		}

		void setBehaviour(KeyFrameAnimationBehaviour * b);
		KeyFrameAnimationBehaviour * getBehaviour();

		/*!	Sets speed factor (>=0) for active animation. 1.0 is standard speed/fps.
			@return false if given value is smaller zero. */
		bool setSpeedFactor(const float & sf);

		float getSpeedFactor(){
			return speedFactor;
		}

		void setState(const short & value);
		short getState();

		/*!	Sets current position of active animation. Receives floats >= 0. Only decimal places are considered.
		 *  So a value of 3.76 results in the same animation position than the value 0.76.
		 *	@return false if given value is smaller zero.
		 */
		bool setAnimationPosition(const float & value);

		/*! Returns the current (last set) animation position. Here only decimal places are considered.
		 *  @return float >= 0.0 < 1.0
		 */
		float getAnimationPosition();


	private:
		/// ---|> [GeometryNode]
		KeyFrameAnimationNode * doClone()const override;

		KeyFrameAnimationData * keyFrameAnimationData;
		KeyFrameAnimationBehaviour * keyFrameAnimationBehaviour;

		float lastTimeStamp;
		float curTime;
		float speedFactor;

		short curState;

		float curAnimationPosition;

		std::pair<std::string, std::vector<int> > activeAnimation;

		Rendering::VertexDescription vertexDescription;

};

}


#endif /* KEYFRAMEANIMATIONNODE_H_ */
