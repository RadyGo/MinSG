/*
	This file is part of the MinSG library.
	Copyright (C) 2007-2012 Benjamin Eikel <benjamin@eikel.org>
	Copyright (C) 2007-2012 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2007-2012 Ralf Petring <ralf@petring.net>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifdef MINSG_EXT_EVALUATORS

#include "AdaptCullEvaluator.h"
#include "../../Core/Nodes/GeometryNode.h"
#include "../../Core/FrameContext.h"
#include "../../Helper/StdNodeVisitors.h"
#include <Geometry/Box.h>
#include <Rendering/RenderingContext/RenderingParameters.h>
#include <Rendering/RenderingContext/RenderingContext.h>
#include <Rendering/Shader/Shader.h>
#include <Rendering/Draw.h>
#include <Rendering/OcclusionQuery.h>
#include <Util/Graphics/Color.h>
#include <Util/GenericAttribute.h>

#include <list>

namespace MinSG {
namespace Evaluators {

// CL:
// DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY //
// DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY //
// DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY //
// DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY DUMMY //

//! [ctor]
AdaptCullEvaluator::AdaptCullEvaluator(DirectionMode _mode/*=SINGLE_VALUE*/):Evaluator(_mode) {
//    if (!whiteShader) {
//        whiteShader=Shader::createShader("void main(void){gl_Position=ftransform();}",
//                    "void main(void){gl_FragColor=vec4(1.0,1.0,1.0,1.0);}");
// //       whiteShader=Shader::loadShader("shader/Simple.vert","shader/Simple.frag");
//    }
}

//! [dtor]
AdaptCullEvaluator::~AdaptCullEvaluator() {
}

//! ---|> Evaluator
void AdaptCullEvaluator::beginMeasure() {
	visibleObjects.clear();
	objectsInVF.clear();
	values->clear();
	setMaxValue_i(0);
}

//! ---|> Evaluator
void AdaptCullEvaluator::measure(FrameContext & context,Node & node,const Geometry::Rect & /*r*/) {
	// first pass - fill depth buffer.
	context.getRenderingContext().clearScreen(Util::Color4f(0.9f,0.9f,0.9f,1.0f));
	node.display(context,USE_WORLD_MATRIX|FRUSTUM_CULLING);

	if (getMaxValue()->toInt() == 0.0) {
		setMaxValue_i(collectNodes<GeometryNode>(&node).size());
	}

	if (mode==SINGLE_VALUE) {

		// collect geometry nodes in frustum
		auto objectsInVFList = collectNodesInFrustum<GeometryNode>(&node, context.getCamera()->getFrustum());
		for(const auto & geoNode : objectsInVFList) {
			objectsInVF[reinterpret_cast<uintptr_t>(geoNode)] = geoNode;
		}


		// setup occlusion queries
		int numQueries=objectsInVFList.size();
		if (numQueries==0) return;

		auto queries = new Rendering::OcclusionQuery[numQueries];

		{ // optional second pass: test bounding boxes
			context.getRenderingContext().pushAndSetDepthBuffer(Rendering::DepthBufferParameters(true, true, Rendering::Comparison::LEQUAL));
			int i=0;
			// start queries
			Rendering::OcclusionQuery::enableTestMode(context.getRenderingContext());
			for(const auto & geoNode : objectsInVFList) {
				queries[i].begin();
//				queries[i].fastBoxTest((*it)->getWorldBB());
				Rendering::drawAbsBox(context.getRenderingContext(), geoNode->getWorldBB() );
				queries[i].end();
				++i;
			}
			Rendering::OcclusionQuery::disableTestMode(context.getRenderingContext());
			i=0;
			// get results
			for(auto & geoNode : objectsInVFList) {
				if (queries[i].getResult() == 0 && !geoNode->getWorldBB().contains(context.getCamera()->getWorldOrigin()) ) {
					geoNode = nullptr;
				}
				++i;
			}
			context.getRenderingContext().popDepthBuffer();
		}


		{ // third pass: test if nodes are exactly visible
			context.getRenderingContext().pushAndSetDepthBuffer(Rendering::DepthBufferParameters(true, true, Rendering::Comparison::EQUAL));
			int i=0;
			// start queries
			for(const auto & geoNode : objectsInVFList) {
				if (!geoNode)
					continue;
				queries[i].begin();
				// render object i
				context.displayNode(geoNode, 0);
				queries[i].end();
				++i;
			}
			i=0;
			// get results
			for(const auto & geoNode : objectsInVFList) {
				if (!geoNode)
					continue;
				if (queries[i].getResult() > 0) {
					visibleObjects[reinterpret_cast<uintptr_t>(geoNode)] = geoNode;
				}
				i++;
			}
			context.getRenderingContext().popDepthBuffer();
		}
		delete [] queries;

	} else {//mode == DIRECTION_VALUES
		const auto nodes = collectVisibleNodes(&node, context);
		values->push_back(new Util::_NumberAttribute<float>(nodes.size()));
	}
}

//! ---|> Evaluator
void AdaptCullEvaluator::endMeasure(FrameContext & ) {
	if (mode==SINGLE_VALUE) {
		values->push_back(new Util::_NumberAttribute<float>(visibleObjects.size()));
	}
}

}
}

#endif /* MINSG_EXT_EVALUATORS */
