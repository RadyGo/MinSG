#ifdef MINSG_EXT_THESISSTANISLAW

#include "PhongGI.h"

#include "../../Core/FrameContext.h"

namespace MinSG{
namespace ThesisStanislaw{
  
const std::string PhongGI::_shaderPath = "ThesisStanislaw/shader/";
  
PhongGI::PhongGI() :
  State(),
  _shader(nullptr), _photonSampler(nullptr)
{
  _shader = Rendering::Shader::loadShader(Util::FileName(_shaderPath + "phongGI.vs"), Util::FileName(_shaderPath + "phongGI.fs"), Rendering::Shader::USE_UNIFORMS | Rendering::Shader::USE_GL);
}

State::stateResult_t PhongGI::doEnableState(FrameContext & context, Node * node, const RenderParam & rp){
  auto& rc = context.getRenderingContext();
  
  rc.pushAndSetShader(_shader.get());
  _photonSampler->bindSamplingTexture(rc);
  
  return State::stateResult_t::STATE_OK;
}

void PhongGI::doDisableState(FrameContext & context, Node *, const RenderParam & /*rp*/) {
  auto& rc = context.getRenderingContext();
  
  _photonSampler->unbindSamplingTexture(rc);
  rc.popShader();
}

void PhongGI::setPhotonSampler(PhotonSampler* sampler){
  _photonSampler = sampler;
}

PhongGI * PhongGI::clone() const {
  return new PhongGI(*this);
}

PhongGI::~PhongGI(){}

}
}


#endif // MINSG_EXT_THESISSTANISLAW
