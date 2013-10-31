/*
	This file is part of the MinSG library.
	Copyright (C) 2007-2012 Benjamin Eikel <benjamin@eikel.org>
	Copyright (C) 2007-2012 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2007-2012 Ralf Petring <ralf@petring.net>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef MINSG_POLYGONMODESTATE_H
#define MINSG_POLYGONMODESTATE_H

#include "RenderingParametersState.h"
#include <Rendering/RenderingContext/ParameterStructs.h>

namespace MinSG {

class PolygonModeState: public RenderingParametersState<Rendering::PolygonModeParameters> {
		PROVIDES_TYPE_NAME(PolygonModeState)
	public:
		PolygonModeState() :
			RenderingParametersState<Rendering::PolygonModeParameters>() {}
		explicit PolygonModeState(const Rendering::PolygonModeParameters & newParameter) :
			RenderingParametersState<Rendering::PolygonModeParameters>(newParameter) {}
		PolygonModeState(const PolygonModeState & other) :
			RenderingParametersState<Rendering::PolygonModeParameters>(other) {}
		virtual ~PolygonModeState() {}

		PolygonModeState * clone() const override{
			return new PolygonModeState(*this);
		}

	private:
		stateResult_t doEnableState(FrameContext & context, Node * node, const RenderParam & rp) override;
		void doDisableState(FrameContext & context, Node * node, const RenderParam & rp) override;
};

}

#endif /* MINSG_POLYGONMODESTATE_H */
