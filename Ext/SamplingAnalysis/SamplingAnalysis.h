/*
	This file is part of the MinSG library extension SamplingAnalysis.
	Copyright (C) 2011-2012 Claudius Jähn <claudius@uni-paderborn.de>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifdef MINSG_EXT_SAMPLING_ANALYSIS

#ifndef POINTANALYZER_H
#define POINTANALYZER_H

#include <vector>
#include <Geometry/Vec3.h>
namespace Util{
class Bitmap;
}
namespace MinSG {

namespace SamplingAnalysis{

struct Histogram1D{
	std::vector<uint32_t> buckets;
	uint32_t sum;
	float maxValue;
	uint32_t overflow;
	explicit Histogram1D( const size_t size):buckets(size),sum(0),maxValue(0),overflow(0){}
};

Histogram1D * createDistanceHistogram(const std::vector<Geometry::Vec3> & positions,const uint32_t numBuckets,float maxDistance=-1);
Histogram1D * createAngleHistogram(const std::vector<Geometry::Vec3> & positions,const uint32_t numBuckets);
Histogram1D * createClosestPointDistanceHistogram(const std::vector<Geometry::Vec3> & positions,const uint32_t numBuckets);
Util::Bitmap * create2dDistanceHistogram(const std::vector<Geometry::Vec3> & positions,const uint32_t numBuckets);

}
}


#endif // POINTANALYZER_H

#endif // MINSG_EXT_SAMPLING_ANALYSIS
