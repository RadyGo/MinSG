/*
	This file is part of the MinSG library extension BlueSurfels.
	Copyright (C) 2014 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2016-2018 Sascha Brandt <sascha@brandt.graphics>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifdef MINSG_EXT_BLUE_SURFELS

#include "SurfelAnalysis.h"

#include "../../Core/FrameContext.h"
#include "../../Core/Nodes/CameraNode.h"
#include "../../Core/Transformations.h"

#include <Rendering/Mesh/Mesh.h>
#include <Rendering/Mesh/VertexAttributeAccessors.h>
#include <Rendering/Mesh/VertexAttributeIds.h>
#include <Geometry/Point.h>
#include <Geometry/PointOctree.h>
#include <Geometry/Box.h>
#include <Geometry/Plane.h>
#include <Geometry/Tools.h>

#include <Util/GenericAttribute.h>
#include <Util/Graphics/Bitmap.h>
#include <Util/Graphics/PixelAccessor.h>

#include <algorithm>
#include <numeric>

namespace MinSG{
namespace BlueSurfels {
using namespace Rendering;
using namespace Geometry;
	
static const Util::StringIdentifier SURFEL_ATTRIBUTE("surfels");
static const Util::StringIdentifier SURFEL_PACKING_ATTRIBUTE("surfelPacking");
static const Util::StringIdentifier SURFEL_SURFACE_ATTRIBUTE("surfelSurface");
static const Util::StringIdentifier SURFEL_MEDIAN_ATTRIBUTE("surfelMedianDist");
static const Util::StringIdentifier SURFEL_FIRST_K_ATTRIBUTE("surfelFirstK");

static const Geometry::Vec3 X_AXIS(1,0,0);
static const Geometry::Vec3 Y_AXIS(0,1,0);
static const Geometry::Vec3 Z_AXIS(0,0,1);

// -------------------
	
struct Sample {
	Sample(uint32_t i, const Vec3& p) : i(i), p(p), n({0,0,0}), t({0,0,0}), w(1) {}
	Sample(uint32_t i, const Vec3& p, const Vec3& n, const Vec3& t) : i(i), p(p), n(n), t(t), w(1) {}
	const Vec3& getPosition() const { return p; }
	uint32_t i; // index
	Vec3 p; // position
	Vec3 n; // normal
	Vec3 t; // tangent
	float w; // weight
};

// approximate the geodetic differential by rotating the differential into the tangent plane
Vec3 getDiff(const Sample& s1, const Sample& s2, bool geodetic) {
	Vec3 d = (s1.p - s2.p) * 2.0 / (s1.w + s2.w);
	
	if(!geodetic)
		return d;
	
	float dist = d.length();
	
	if(s1.n.isZero() || s2.n.isZero()) {
		std::cerr << "Zero Normal AAAAAAAHHH!" << std::endl;
		return {0,0,0};
	}
	
	// approximate geodetic distance (Bowers 2010)
	Vec3 v = d / dist;
	float c1 = std::max(-1.0f,std::min(1.0f, s1.n.dot(v))); // clamp to [-1,1]
	float c2 = std::max(-1.0f,std::min(1.0f, s2.n.dot(v))); // clamp to [-1,1]
	if(c1 == c2)
		dist *= std::sqrt(1.0f - c1*c1);
	else 
		dist *= (std::asin(c1) - std::asin(c2)) / (c1 - c2);
	
	// compute cotangent
	Vec3 cot = s1.n.cross(s1.t).normalize();
	
	// project differential onto tangent plane
	d.setValue(s1.t.dot(d), cot.dot(d), 0);
	if(d.isZero()) {
		d.setValue(dist, 0, 0); // might be too biased?
	} else {
		d.normalize();
		d *= dist;
	}
	return d;
}

// -------------------

std::vector<float> getProgressiveMinimalMinimalVertexDistances(Rendering::Mesh& mesh){

	Util::Reference<Rendering::PositionAttributeAccessor> positionAccessor(Rendering::PositionAttributeAccessor::create(mesh.openVertexData(), Rendering::VertexAttributeIds::POSITION));
	
	Geometry::Box bb = mesh.getBoundingBox();
	bb.resizeRel( 1.01 );
	
	typedef Geometry::Point<Geometry::Vec3> P;
	Geometry::PointOctree<P> octree(bb,bb.getExtentMax()*0.01,8);
	
	float currentClosestDistance = bb.getDiameter();
	
	const size_t endIndex = mesh.getVertexCount();

	std::vector<float> progressiveClosestDistances;
	progressiveClosestDistances.reserve(endIndex-1);
	
	std::deque<P> closestNeighbors;
	for(size_t vIndex = 0 ; vIndex<endIndex; ++vIndex){
		const auto pos = positionAccessor->getPosition(vIndex);
	
		closestNeighbors.clear();
		octree.getClosestPoints(pos, 1, closestNeighbors);
			
		if(!closestNeighbors.empty()){
			const float d = pos.distance(closestNeighbors.front().getPosition());
			if(d<currentClosestDistance)
				currentClosestDistance = d;
			progressiveClosestDistances.push_back(currentClosestDistance);
		}
			
		octree.insert( P(pos) );
	}
	
	return progressiveClosestDistances;
}

std::vector<float> getMinimalVertexDistances(Rendering::Mesh& mesh,size_t prefixLength, bool geodesic/*=false*/){

	Util::Reference<Rendering::PositionAttributeAccessor> pAcc(Rendering::PositionAttributeAccessor::create(mesh.openVertexData(), Rendering::VertexAttributeIds::POSITION));
	
	Util::Reference<Rendering::NormalAttributeAccessor> nAcc;
	Util::Reference<Rendering::NormalAttributeAccessor> tAcc;
	if(geodesic && !mesh.getVertexDescription().hasAttribute(VertexAttributeIds::NORMAL)) {
		WARN("getMinimalVertexDistances requires normals when used with geodesics");
		geodesic = false;
	}
	if(geodesic) {
		nAcc = Rendering::NormalAttributeAccessor::create(mesh.openVertexData());
		if(mesh.getVertexDescription().hasAttribute(VertexAttributeIds::TANGENT)) {
			// Yaay, we have tangents
			tAcc = Rendering::NormalAttributeAccessor::create(mesh.openVertexData(), VertexAttributeIds::TANGENT);
		}
	}
	
	// collect samples and create octree
	std::deque<Sample> samples;
	Geometry::Box bb = mesh.getBoundingBox();
	bb.resizeRel( 1.01 );	
	Geometry::PointOctree<Sample> octree(bb,bb.getExtentMax()*0.01,8);
	prefixLength = prefixLength > 0 ? std::min<size_t>(prefixLength, mesh.getVertexCount()) : mesh.getVertexCount();
	for(uint32_t i=0; i<prefixLength; ++i) {
		auto p = pAcc->getPosition(i);
		if(geodesic) {
			auto n = nAcc->getNormal(i);
			if(n.isZero()) {
				std::cerr << "Zero Normal " << i << " " << p << " " << n << std::endl;
				continue;
			}
			n.normalize();
			Vec3 t;
			if(tAcc.isNull()) {
				// use arbitrary basis to compute tangents
				t = n.cross(X_AXIS);
				if(t.isZero()) t = n.cross(Y_AXIS);
				if(t.isZero()) t = n.cross(Z_AXIS);
				t.normalize();
			} else {
				t = tAcc->getNormal(i);
			}
			samples.emplace_back(i, p, n, t);
		} else {
			samples.emplace_back(i, p);
		}
		octree.insert(samples.back());
	}	
		
	std::vector<float> closestDistances;
	closestDistances.reserve(prefixLength);
	std::deque<Sample> neighbors;
	
	for(auto& s1 : samples) {
		neighbors.clear();
		octree.getClosestPoints(s1.p, 2, neighbors); // this point and closest neighbor
		if(neighbors.size()==2){ // get other point
			closestDistances.push_back(getDiff(s1, neighbors[0].i==s1.i ? neighbors[1] : neighbors[0], geodesic).length());
		}
	}
	
	return closestDistances;
}

float getMedianOfNthClosestNeighbours(Rendering::Mesh& mesh, size_t prefixLength, size_t nThNeighbour){
	if(mesh.getVertexCount()<=nThNeighbour)
		return 0;
	
	auto positionAccessor = Rendering::PositionAttributeAccessor::create(mesh.openVertexData(), Rendering::VertexAttributeIds::POSITION);
	const size_t endIndex = std::min(static_cast<size_t>(mesh.getVertexCount()),prefixLength);
	
	
	struct OctreeEntry : public Geometry::Point<Geometry::Vec3f> {
		size_t surfelId;
		OctreeEntry(size_t i,const Geometry::Vec3 & p) : Geometry::Point<Geometry::Vec3f>(p), surfelId(i) {}
	};

	const auto bb = mesh.getBoundingBox();
	Geometry::PointOctree<OctreeEntry> octree(bb,bb.getExtentMax()*0.01,8);
	
	std::deque<OctreeEntry> closestNeighbours;
	for(size_t vIndex = 0; vIndex<endIndex; ++vIndex)
		octree.insert( OctreeEntry(vIndex, positionAccessor->getPosition(vIndex)) );

	std::vector<float> nThClosestDistances;

	std::vector<float> distances;
	for(size_t vIndex = 0; vIndex<endIndex; ++vIndex){
		distances.clear();
		
		const auto pos = positionAccessor->getPosition(vIndex);
	
		closestNeighbours.clear();
		octree.getClosestPoints(pos, nThNeighbour+1, closestNeighbours);

		for(const auto& neighbour : closestNeighbours)
			distances.push_back( neighbour.getPosition().distance( pos ) );
		
		std::sort(distances.begin(), distances.end());
		nThClosestDistances.push_back( distances[nThNeighbour] );
	}

	std::sort(nThClosestDistances.begin(), nThClosestDistances.end());
	return nThClosestDistances[ nThClosestDistances.size()*0.5 ];
}

float computeRelPixelSize(AbstractCameraNode* camera, MinSG::Node * node, ReferencePoint ref) {
	static const Geometry::Vec3 Z_AXIS(0,0,1);
	const auto& vp = camera->getViewport();
	const auto& modelToWorld = node->getWorldTransformationMatrix();
	const auto cam_pos_ws = camera->getWorldOrigin();
	const auto cam_dir_ws = camera->getWorldTransformationSRT().getDirVector();
	
	std::vector<Geometry::Vec3> firstK_ws;
	
	switch(ref) {
		case ReferencePoint::FARTHEST_BB:
			firstK_ws.emplace_back(node->getWorldBB().getClosestPoint(cam_pos_ws - cam_dir_ws*camera->getFarPlane()));
			break;
		case ReferencePoint::CENTER_BB:
			firstK_ws.emplace_back(node->getWorldBB().getCenter());
			break;
		case ReferencePoint::CLOSEST_SURFEL: {
			auto firstKAttr = dynamic_cast<Util::GenericAttributeList*>(node->findAttribute(SURFEL_FIRST_K_ATTRIBUTE));
			auto* surfels = getSurfels(node);
			if(!surfels) {
				firstK_ws.emplace_back(node->getWorldBB().getClosestPoint(cam_pos_ws));
			} else if(firstKAttr) {
				for(uint_fast8_t i=0; i<firstKAttr->size()/3; ++i) {
					firstK_ws.emplace_back(modelToWorld.transformPosition(
						firstKAttr->at(3*i+0)->toFloat(),
						firstKAttr->at(3*i+1)->toFloat(),
						firstKAttr->at(3*i+2)->toFloat()
					));
				}
			} else {
				auto posAcc = Rendering::PositionAttributeAccessor::create(surfels->openVertexData());
				firstKAttr = new Util::GenericAttributeList;
				for(uint_fast8_t i=0; i<std::min(8u,surfels->getVertexCount()); ++i) {
					const auto pos = posAcc->getPosition(i); 
					firstK_ws.emplace_back(modelToWorld.transformPosition(pos));
					firstKAttr->push_back(Util::GenericAttribute::createNumber(pos.x()));
					firstKAttr->push_back(Util::GenericAttribute::createNumber(pos.y()));
					firstKAttr->push_back(Util::GenericAttribute::createNumber(pos.z()));
				}
				auto* proto = node->isInstance() ? node->getPrototype() : node;
				proto->setAttribute(SURFEL_FIRST_K_ATTRIBUTE, firstKAttr);
			}
			break;
		}
		default:
			firstK_ws.emplace_back(node->getWorldBB().getClosestPoint(cam_pos_ws));
	}
	
	float dist_ws = camera->getFarPlane() + 1;
	for(const auto& p : firstK_ws)
		dist_ws = std::min(dist_ws, (cam_pos_ws - p).dot(cam_dir_ws));
	dist_ws = camera->getFrustum().isOrthogonal() ? 1 : std::max(camera->getNearPlane(), dist_ws);
	
	const auto l = camera->getFrustum().getLeft();
	const auto r = camera->getFrustum().getRight();
	const auto n = camera->getNearPlane();
	const auto w = static_cast<float>(vp.getWidth());
	const auto s = node->getWorldTransformationSRT().getScale();
	return ((r-l) * dist_ws) / (n * w * s);
}

float computeSurfelPacking(Rendering::Mesh* mesh) {
	if(!mesh) return 0;
	uint32_t count = std::min(1000u, mesh->getVertexCount());
	//float r = getMedianOfNthClosestNeighbours(*mesh, count, 1);
	auto dist = getMinimalVertexDistances(*mesh, count);
	//float r = *std::min_element(dist.begin(), dist.end());
	float r = std::accumulate(dist.begin(), dist.end(), 0.0)/count;
	return r * r * static_cast<float>(count);
}

float getSurfelPacking(MinSG::Node* node, Rendering::Mesh* surfels) {
	if(!surfels)
		return 0;
	if(node->isInstance())
		node = node->getPrototype();
		
	auto surfelPackingAttr = node->findAttribute(SURFEL_PACKING_ATTRIBUTE);
	if(surfelPackingAttr)
		return surfelPackingAttr->toFloat();
		
	auto surfelSurfaceAttr = node->findAttribute(SURFEL_SURFACE_ATTRIBUTE);
	if(surfelSurfaceAttr) {
		float packing = surfelSurfaceAttr->toFloat();
		node->unsetAttribute(SURFEL_SURFACE_ATTRIBUTE);
		node->setAttribute(SURFEL_PACKING_ATTRIBUTE, Util::GenericAttribute::createNumber(packing));
		return packing;
	}
	
	// try to find deprecated 'surfelMedianDist' attribute & compute surface 
	auto surfelMedianAttr = node->findAttribute(SURFEL_MEDIAN_ATTRIBUTE);
	if(surfelMedianAttr) {
		uint32_t medianCount = std::min(1000U, surfels->getVertexCount());
		float median = surfelMedianAttr->toFloat();
		float packing = static_cast<float>(medianCount) * median * median;  
		node->unsetAttribute(SURFEL_MEDIAN_ATTRIBUTE);
		node->setAttribute(SURFEL_PACKING_ATTRIBUTE, Util::GenericAttribute::createNumber(packing));
		return packing;
	}

	float packing = computeSurfelPacking(surfels);
	node->setAttribute(SURFEL_PACKING_ATTRIBUTE, Util::GenericAttribute::createNumber(packing));
	return packing;
}

Rendering::Mesh* getSurfels(MinSG::Node * node) {
	auto surfelAttribute = dynamic_cast<Util::ReferenceAttribute<Rendering::Mesh>*>(node->findAttribute( SURFEL_ATTRIBUTE ));
	return surfelAttribute ? surfelAttribute->get() : nullptr;
}

// -------------------

Util::Reference<Util::Bitmap> differentialDomainAnalysis(Rendering::Mesh* mesh, float diff_max, int32_t resolution, uint32_t count, bool geodetic, bool adaptive) {
	
	// mesh
	count = count > 0 ? std::min(count, mesh->getVertexCount()) : mesh->getVertexCount();
	auto pAcc = Rendering::PositionAttributeAccessor::create(mesh->openVertexData());
	Util::Reference<Rendering::NormalAttributeAccessor> nAcc;
	Util::Reference<Rendering::NormalAttributeAccessor> tAcc;
	Util::Reference<Rendering::ColorAttributeAccessor> cAcc;
	if(geodetic && !mesh->getVertexDescription().hasAttribute(VertexAttributeIds::NORMAL)) {
		WARN("Differential domain analysis requires normals for analysis with geodesics");
		geodetic = false;
	}
	if(geodetic) {
		nAcc = Rendering::NormalAttributeAccessor::create(mesh->openVertexData());
		if(mesh->getVertexDescription().hasAttribute(VertexAttributeIds::TANGENT)) {
			// Yaay, we have tangents
			tAcc = Rendering::NormalAttributeAccessor::create(mesh->openVertexData(), VertexAttributeIds::TANGENT);
		}
	}
	
	if(adaptive && !mesh->getVertexDescription().hasAttribute(VertexAttributeIds::COLOR)) {
		WARN("Differential domain analysis uses the color as weight for adaptive samples");
		adaptive = false;
	}	
	if(adaptive) {
		cAcc = Rendering::ColorAttributeAccessor::create(mesh->openVertexData());
	}
	Geometry::Box bb = mesh->getBoundingBox();
	
	// parameter
	//Sphere_f sphere({0.0,0.0,0.0}, diff_max);
	Box queryBox({0,0,0}, diff_max*2*std::sqrt(2));
	const float cell_size = 2*diff_max/resolution;
	const int kernel_size = 4; // size of the gaussian convolution kernel
	const Vec2i imgCenter(resolution>>1,resolution>>1);
	const Rect_i imageDim(0,0,resolution-1,resolution-1);
	std::vector<std::vector<float>> spec(resolution, std::vector<float>(resolution, 0)); // spectral map

	// collect samples and create octree
	std::deque<Sample> neighbors;
	std::deque<Sample> samples;
	bb.resizeRel( 1.01 );
	Geometry::PointOctree<Sample> octree(bb,bb.getExtentMax()*0.01,8);
	for(uint32_t i=0; i<count; ++i) {
		auto p = pAcc->getPosition(i);
		if(geodetic) {
			auto n = nAcc->getNormal(i);
			if(n.isZero()) {
				std::cerr << "Zero Normal " << i << " " << p << " " << n << std::endl;
				continue;
			}
			Vec3 t;
			if(tAcc.isNull()) {
				// use arbitrary basis to compute tangents
				t = n.cross(X_AXIS);
				if(t.isZero()) t = n.cross(Y_AXIS);
				if(t.isZero()) t = n.cross(Z_AXIS);
				t.normalize();
			} else {
				t = tAcc->getNormal(i);
			}
			samples.emplace_back(i, p, n, t);
		} else {
			samples.emplace_back(i, p);
		}
		if(adaptive) {
			samples.back().w = cAcc->getColor4f(i).r();
		}
		octree.insert(samples.back());
	}
	
	// compute differentials of samples and scatter to spectrum map
	std::vector<Vec2> diffs;
	Vec2 query;
	Vec2i diffIndex;
	//float max = 0;
	float totalSum = 0;
	for(auto& s1 : samples) {
		neighbors.clear();
		diffs.clear();
		queryBox.setCenter(s1.p);
		octree.collectPointsWithinBox(queryBox, neighbors);
		
		// gather differentials
		for(auto& s2 : neighbors) {
			if(s1.i == s2.i) continue;
			Vec3 diff = getDiff(s1, s2, geodetic);
			if(diff.isZero()) continue;			
			diffs.emplace_back(diff.x(), diff.y());
		}
		
		// scatter
		for(auto& diff : diffs) {
			diffIndex = Vec2i(diff/cell_size) + imgCenter;
			
			// gaussian convolution
			for(int x=-kernel_size; x<=kernel_size; ++x) {
				for(int y=-kernel_size; y<=kernel_size; ++y) {
					Vec2i index = diffIndex + Vec2i(x, y);
					if(imageDim.contains(index)) {
						query = Vec2(index - imgCenter) * cell_size;
						const float dist = diff.distanceSquared(query);
						float value = std::exp(-dist/(cell_size*cell_size));
						spec[index.x()][index.y()] += value;
						//totalSum += value;
						//max = std::max(max, spec[index.x()][index.y()]);
					}
				}
			}
		}
	}
	
	for(uint32_t x=0; x<resolution; ++x) {
		for(uint32_t y=0; y<resolution; ++y) {
			totalSum += spec[x][y];
		}
	}
	
	// normalize and write to bitmap
	float normalizationFactor = (resolution*resolution) / totalSum;
	Util::Reference<Util::Bitmap> result(new Util::Bitmap(resolution,resolution,Util::PixelFormat::RGB_FLOAT));
	auto resultAcc = Util::PixelAccessor::create(result);
	for(uint32_t x=0; x<resolution; ++x) {
		for(uint32_t y=0; y<resolution; ++y) {
			//float ps = normalize ? (spec[x][y]/max) : spec[x][y];
			float ps = spec[x][y] * normalizationFactor;
			resultAcc->writeColor(x, y, Util::Color4f(ps,ps,ps));
		}
	}
	return result;
}

std::vector<Radial> getRadialMeanVariance(const Util::Reference<Util::Bitmap>& spectrum) {
	std::vector<Radial> result(spectrum->getWidth()/2+1, {0,0,0});
	auto acc = Util::PixelAccessor::create(spectrum);
	
	for(uint32_t x=0; x<spectrum->getWidth(); ++x) {
		for(uint32_t y=0; y<spectrum->getHeight(); ++y) {
			Geometry::Vec2 v(x-spectrum->getWidth()/2.0, y-spectrum->getHeight()/2.0);
			uint32_t entry = v.length();
			if(entry < result.size()) {
				float value = acc->readSingleValueFloat(x, y);
				result[entry].mean += value;
				result[entry].count++;
			}
		}
	}
	
	for(uint32_t i=0; i<result.size(); ++i)
		result[i].mean /= result[i].count;
		
	for(uint32_t x=0; x<spectrum->getWidth(); ++x) {
		for(uint32_t y=0; y<spectrum->getHeight(); ++y) {
			Geometry::Vec2 v(x-spectrum->getWidth()/2.0, y-spectrum->getHeight()/2.0);
			uint32_t entry = v.length();
			if(entry < result.size()) {
				float var = acc->readSingleValueFloat(x, y) - result[entry].mean;
				result[entry].variance += var*var;
			}
		}
	}
	for(uint32_t i=0; i<result.size(); ++i) {
		if(result[i].count > 1 && result[i].mean > 0)
			result[i].variance /= result[i].mean * result[i].mean * (result[i].count-1);
	}
	return result;
}

}
}
#endif // MINSG_EXT_BLUE_SURFELS
