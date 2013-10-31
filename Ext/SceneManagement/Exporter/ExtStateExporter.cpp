/*
	This file is part of the MinSG library.
	Copyright (C) 2007-2012 Benjamin Eikel <benjamin@eikel.org>
	Copyright (C) 2007-2012 Claudius Jähn <claudius@uni-paderborn.de>
	Copyright (C) 2007-2012 Ralf Petring <ralf@petring.net>
	
	This library is subject to the terms of the Mozilla Public License, v. 2.0.
	You should have received a copy of the MPL along with this library; see the 
	file LICENSE. If not, you can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "ExtStateExporter.h"

#include "../ExtConsts.h"

#include "../../../SceneManagement/SceneDescription.h"
#include "../../../SceneManagement/SceneManager.h"
#include "../../../SceneManagement/Exporter/ExporterTools.h"

#include "../../OcclusionCulling/OccRenderer.h"
#include "../../OcclusionCulling/CHCppRenderer.h"
#include "../../States/BudgetAnnotationState.h"
#include "../../States/MirrorState.h"
#include "../../States/ProjSizeFilterState.h"
#include "../../States/LODRenderer.h"

#ifdef MINSG_EXT_SKELETAL_ANIMATION
#include "../../SkeletalAnimation/Renderer/SkeletalHardwareRendererState.h"
#include "../../SkeletalAnimation/Renderer/SkeletalSoftwareRendererState.h"
#endif

#ifdef MINSG_EXT_COLORCUBES
#include "../../ColorCubes/ColorCubeRenderer.h"
#endif

#ifdef MINSG_EXT_MULTIALGORENDERING
#include "../../MultiAlgoRendering/AlgoSelector.h"
#include "../../MultiAlgoRendering/SurfelRenderer.h"
#include <Util/IO/FileName.h>
#include <Util/IO/FileUtils.h>
#endif

#ifdef MINSG_EXT_SPHERICALSAMPLING
#include "../../SphericalSampling/BudgetRenderer.h"
#include "../../SphericalSampling/Helper.h"
#include "../../SphericalSampling/Renderer.h"
#endif

#include <cassert>

namespace MinSG {
namespace SceneManagement {

static void describeOccRenderer(ExporterContext &,NodeDescription & desc,State *) {
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_OCC_RENDERER);
}

static void describeCHCppRenderer(ExporterContext &,NodeDescription & desc,State * state) {
	auto chcpp = dynamic_cast<CHCppRenderer *>(state);
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_CHCPP_RENDERER);
	desc.setValue(Consts::ATTR_CHCPP_VISIBILITYTHRESHOLD, Util::GenericAttribute::createNumber<uint16_t>(chcpp->getVisibilityThreshold()));
	desc.setValue(Consts::ATTR_CHCPP_MAXPREVINVISNODESBATCHSIZE, Util::GenericAttribute::createNumber<uint16_t>(chcpp->getMaxPrevInvisNodesBatchSize()));
	desc.setValue(Consts::ATTR_CHCPP_SKIPPEDFRAMESTILLQUERY, Util::GenericAttribute::createNumber<uint16_t>(chcpp->getSkippedFramesTillQuery()));
	desc.setValue(Consts::ATTR_CHCPP_MAXDEPTHFORTIGHTBOUNDINGVOLUMES, Util::GenericAttribute::createNumber<uint16_t>(chcpp->getMaxDepthForTightBoundingVolumes()));
	desc.setValue(Consts::ATTR_CHCPP_MAXAREADERIVATIONFORTIGHTBOUNDINGVOLUMES, Util::GenericAttribute::createNumber<float>(chcpp->getMaxAreaDerivationForTightBoundingVolumes()));
}

static void describeBudgetAnnotationState(ExporterContext &,NodeDescription & desc,State * state) {
	auto bas = dynamic_cast<BudgetAnnotationState *>(state);
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_BUDGET_ANNOTATION_STATE);
	desc.setValue(Consts::ATTR_BAS_ANNOTATION_ATTRIBUTE, Util::GenericAttribute::create(bas->getAnnotationAttribute().toString()));
	desc.setValue(Consts::ATTR_BAS_BUDGET, Util::GenericAttribute::create(bas->getBudget()));
	desc.setValue(Consts::ATTR_BAS_DISTRIBUTION_TYPE, Util::GenericAttribute::create(BudgetAnnotationState::distributionTypeToString(bas->getDistributionType())));
}

static void describeMirrorState(ExporterContext &,NodeDescription & desc,State * state) {
	auto ms = dynamic_cast<MirrorState *>(state);
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_MIRROR_STATE);
	desc.setValue(Consts::ATTR_MIRROR_TEXTURE_SIZE, Util::GenericAttribute::createNumber<uint16_t>(ms->getTextureSize()));
}

static void describeProjSizeFilterState(ExporterContext &,NodeDescription & desc,State * state) {
	auto psfs = dynamic_cast<ProjSizeFilterState *>(state);
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_PROJ_SIZE_FILTER_STATE);
	desc.setValue(Consts::ATTR_PSFS_MAXIMUM_PROJECTED_SIZE, Util::GenericAttribute::createNumber(psfs->getMaximumProjSize()));
	desc.setValue(Consts::ATTR_PSFS_MINIMUM_DISTANCE, Util::GenericAttribute::createNumber(psfs->getMinimumDistance()));
	desc.setValue(Consts::ATTR_PSFS_SOURCE_CHANNEL, Util::GenericAttribute::createString(psfs->getSourceChannel().toString()));
	desc.setValue(Consts::ATTR_PSFS_TARGET_CHANNEL, Util::GenericAttribute::createString(psfs->getTargetChannel().toString()));
	desc.setValue(Consts::ATTR_PSFS_FORCE_CLOSED_NODES, Util::GenericAttribute::createBool(psfs->isForceClosed()));
}

#ifdef MINSG_EXT_COLORCUBES
static void describeColorCubeRenderer(ExporterContext &,NodeDescription & desc,State * state) {
	auto cr = dynamic_cast<ColorCubeRenderer*>(state);
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_COLOR_CUBE_RENDERER);
	desc.setString(Consts::ATTR_COLOR_CUBE_RENDERER_HIGHLIGHT, Util::StringUtils::toString(cr->isHighlightEnabled()));
}
#endif

#ifdef MINSG_EXT_SKELETAL_ANIMATION
static void describeSkeletalRendererState(ExporterContext &,NodeDescription & desc,State * state) {
	auto srs = dynamic_cast<SkeletalAbstractRendererState *>(state);
	std::stringstream ss;
	ss << srs->getBindMatrix();
	desc.setString(Consts::ATTR_SKEL_BINDMATRIX, ss.str());
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_SKEL_SKELETALRENDERERSTATE);
}
#endif

#ifdef MINSG_EXT_MULTIALGORENDERING
static void describeAlgoSelector(ExporterContext & ctxt,NodeDescription & desc,State * state) {
	auto as = dynamic_cast<MAR::AlgoSelector*>(state);
	assert(as);
	
	desc.setValue(Consts::TYPE, Util::GenericAttribute::createString(Consts::TYPE_STATE));
	desc.setValue(Consts::ATTR_STATE_TYPE,Util::GenericAttribute::createString(Consts::STATE_TYPE_ALGOSELECTOR));

	Util::FileName file(ctxt.sceneFile);
	file.setEnding(".mar");
	std::unique_ptr<std::ostream> out(Util::FileUtils::openForWriting(file));
	as->write(*(out.get()));
}
static void describeMARSurfelRenderer(ExporterContext &,NodeDescription & desc,State * state) {
	auto sr = dynamic_cast<MAR::SurfelRenderer*>(state);
	assert(sr);
	
	desc.setValue(Consts::TYPE, Util::GenericAttribute::createString(Consts::TYPE_STATE));
	desc.setValue(Consts::ATTR_STATE_TYPE,Util::GenericAttribute::createString(Consts::STATE_TYPE_MAR_SURFEL_RENDERER));
	desc.setValue(Consts::ATTR_MAR_SURFEL_COUNT_FACTOR, Util::GenericAttribute::createNumber<float>(sr->getSurfelCountFactor()));
	desc.setValue(Consts::ATTR_MAR_SURFEL_SIZE_FACTOR, Util::GenericAttribute::createNumber<float>(sr->getSurfelSizeFactor()));
	desc.setValue(Consts::ATTR_MAR_SURFEL_MAX_AUTO_SIZE, Util::GenericAttribute::createNumber<float>(sr->getMaxAutoSurfelSize()));
	desc.setValue(Consts::ATTR_MAR_SURFEL_FORCE_FLAG, Util::GenericAttribute::createBool(sr->getForceSurfels()));
}
#endif

#ifdef MINSG_EXT_SPHERICALSAMPLING
static void describeSVSRenderer(ExporterContext &, NodeDescription & desc, State * state) {
	auto renderer = dynamic_cast<SphericalSampling::Renderer *>(state);
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_SVS_RENDERER);
	desc.setString(Consts::ATTR_SVS_INTERPOLATION_METHOD, SphericalSampling::interpolationToString(renderer->getInterpolationMethod()));
	desc.setValue(Consts::ATTR_SVS_RENDERER_SPHERE_OCCLUSION_TEST, Util::GenericAttribute::createBool(renderer->isSphereOcclusionTestEnabled()));
	desc.setValue(Consts::ATTR_SVS_RENDERER_GEOMETRY_OCCLUSION_TEST, Util::GenericAttribute::createBool(renderer->isGeometryOcclusionTestEnabled()));
}

static void describeSVSBudgetRenderer(ExporterContext &, NodeDescription & desc, State * state) {
	auto renderer = dynamic_cast<SphericalSampling::BudgetRenderer *>(state);
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_SVS_BUDGETRENDERER);
	desc.setValue(Consts::ATTR_SVS_BUDGETRENDERER_BUDGET, Util::GenericAttribute::createNumber<uint32_t>(renderer->getBudget()));
}
#endif

static void describeLODRenderer(ExporterContext &,NodeDescription & desc,State * state) {
	auto renderer = dynamic_cast<LODRenderer *>(state);
	desc.setString(Consts::ATTR_STATE_TYPE, Consts::STATE_TYPE_LOD_RENDERER);
	desc.setValue(Consts::ATTR_LOD_RENDERER_MAX_COMPLEXITY, Util::GenericAttribute::createNumber(renderer->getMaxComplexity()));
	desc.setValue(Consts::ATTR_LOD_RENDERER_MIN_COMPLEXITY, Util::GenericAttribute::createNumber(renderer->getMinComplexity()));
	desc.setValue(Consts::ATTR_LOD_RENDERER_REL_COMPLEXITY, Util::GenericAttribute::createNumber(renderer->getRelComplexity()));
	desc.setValue(Consts::ATTR_LOD_RENDERER_SOURCE_CHANNEL, Util::GenericAttribute::createString(renderer->getSourceChannel().toString()));
}

void initExtStateExporter(SceneManager & sm) {
	sm.addStateExporter(OccRenderer::getClassId(),&describeOccRenderer);
	sm.addStateExporter(CHCppRenderer::getClassId(),&describeCHCppRenderer);
	sm.addStateExporter(BudgetAnnotationState::getClassId(),&describeBudgetAnnotationState);
	sm.addStateExporter(MirrorState::getClassId(),&describeMirrorState);
	sm.addStateExporter(ProjSizeFilterState::getClassId(),&describeProjSizeFilterState);
	sm.addStateExporter(MinSG::LODRenderer::getClassId(),&describeLODRenderer);

#ifdef MINSG_EXT_MULTIALGORENDERING
	sm.addStateExporter(MAR::AlgoSelector::getClassId(),&describeAlgoSelector);
	sm.addStateExporter(MAR::SurfelRenderer::getClassId(),&describeMARSurfelRenderer);
#endif

#ifdef MINSG_EXT_COLORCUBES
	sm.addStateExporter(ColorCubeRenderer::getClassId(),&describeColorCubeRenderer);
#endif

#ifdef MINSG_EXT_SKELETAL_ANIMATION
	sm.addStateExporter(SkeletalHardwareRendererState::getClassId(),&describeSkeletalRendererState);
	sm.addStateExporter(SkeletalSoftwareRendererState::getClassId(),&describeSkeletalRendererState);
#endif

#ifdef MINSG_EXT_SPHERICALSAMPLING
	sm.addStateExporter(SphericalSampling::Renderer::getClassId(), &describeSVSRenderer);
	sm.addStateExporter(SphericalSampling::BudgetRenderer::getClassId(), &describeSVSBudgetRenderer);
#endif

}

}
}
