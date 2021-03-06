/*
	This file is part of the MinSG library.
	Copyright (C) 2007-2013 Benjamin Eikel <benjamin@eikel.org>
	Copyright (C) 2007-2012 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2007-2012 Ralf Petring <ralf@petring.net>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifdef MINSG_EXT_WAYPOINTS

#ifndef PATHNODE_H
#define PATHNODE_H

#include "../../Core/Nodes/GroupNode.h"
#include "../../Core/Behaviours/AbstractBehaviour.h"
#include <Geometry/Box.h>
#include <Util/References.h>
#include <list>
#include <map>

namespace Rendering {
class Mesh;
}
namespace MinSG {

class Waypoint;

/**
 * PathNode ---|> GroupNode ---|> Node
 * @ingroup nodes
 */
class PathNode : public GroupNode {
		PROVIDES_TYPE_NAME(PathNode)
	public:

		typedef std::map<AbstractBehaviour::timestamp_t, Util::Reference<Waypoint> > wayPointMap_t;
		// ---------------------------------------
		PathNode();
		PathNode(const PathNode & source);
		virtual ~PathNode();

		/*! Create and add a new waypoint */
		Waypoint * createWaypoint(const Geometry::SRT & position,AbstractBehaviour::timestamp_t time);
		void updateWaypoint(Waypoint * wp,AbstractBehaviour::timestamp_t newTime);

		Waypoint * getWaypoint(AbstractBehaviour::timestamp_t time);

		void removeLastWaypoint();

		size_t countWaypoints()const 						{	return waypoints.size();	}
		wayPointMap_t::const_iterator begin()const 			{	return waypoints.begin();	}
		wayPointMap_t::const_iterator end()const 			{	return waypoints.end();	}

		wayPointMap_t::const_iterator getNextWaypoint(AbstractBehaviour::timestamp_t time)const;
		wayPointMap_t::iterator getNextWaypoint(AbstractBehaviour::timestamp_t time);
		Geometry::SRT getPosition(AbstractBehaviour::timestamp_t time)const;
		Geometry::SRT getWorldPosition(AbstractBehaviour::timestamp_t time);
		void closeLoop(AbstractBehaviour::timestamp_t time);

		bool isLooping()const 	{	return looping;	}
		void setLooping(bool b) {	looping=b;	}

		AbstractBehaviour::timestamp_t getMaxTime()const;

		bool isBBValid()const 	{	return bbValid;	}

		// ---|> GroupNode
		size_t countChildren() const override 					{	return countWaypoints();	}

		// ---|> Node
		void doDisplay(FrameContext & context, const RenderParam & rp) override;
		NodeVisitor::status traverse(NodeVisitor & visitor) override;

		bool getMetaDisplayWaypoints() const {
			return metaDisplayWaypoints;
		}
		void setMetaDisplayWaypoints(bool displayWaypoints) {
			metaDisplayWaypoints = displayWaypoints;
		}

		bool getMetaDisplayTimes() const {
			return metaDisplayTimes;
		}
		void setMetaDisplayTimes(bool displayTimes) {
			metaDisplayTimes = displayTimes;
		}

	private:
		// ---|> Node
		const Geometry::Box& doGetBB() const override;
		PathNode * doClone()const override	{	return new PathNode(*this);	}

		// ---|> GroupNode
		void invalidateCompoundBB() override;
		void doAddChild(Util::Reference<Node> child) override;
		bool doRemoveChild(Util::Reference<Node> child) override;

		wayPointMap_t waypoints;
		Util::Reference<Rendering::Mesh> metaMesh;
		bool looping;
		mutable bool bbValid;
		mutable Geometry::Box bb;
		//! Enable/disable the display of the waypoints' meta objects
		bool metaDisplayWaypoints;
		//! Enable/disable the display of the waypoints' time stamps
		bool metaDisplayTimes;
};

}

#endif // PATHNODE_H

#endif /* MINSG_EXT_WAYPOINTS */
