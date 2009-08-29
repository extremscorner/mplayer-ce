#ifndef Renderer_HPP
#define Renderer_HPP

#include "FBO.hpp"
#include "BeatDetect.hpp"
#include <string>
#include <set>

#include "GL/gl.h"
#include "Gekko.h"

#include "Pipeline.hpp"
#include "PerPixelMesh.hpp"
#include "Transformation.hpp"
#include "ShaderEngine.hpp"

class UserTexture;
class BeatDetect;
class TextureManager;

class Renderer
{

public:

  bool studio;
  bool correction;

  bool noSwitch;

  int totalframes;
  float realfps;

  int texsize;


  Renderer( int width, int height, int gx, int gy, int texsize, BeatDetect *beatDetect, std::string presetURL, bool _useWiiLight);
  ~Renderer();

  void RenderFrame(const Pipeline &pipeline, const PipelineContext &pipelineContext);
  void ResetTextures();
  void reset(int w, int h);
  GLuint initRenderToTexture();


  void SetPipeline(Pipeline &pipeline);

  void setPresetName(const std::string& theValue)
  {
    m_presetName = theValue;
  }

  std::string presetName() const
  {
    return m_presetName;
  }

private:

	PerPixelMesh mesh;
  RenderTarget *renderTarget;
  BeatDetect *beatDetect;
  TextureManager *textureManager;
  static Pipeline* currentPipe;
  RenderContext renderContext;
  //per pixel equation variables
#ifdef USE_CG
  ShaderEngine shaderEngine;
#endif
  std::string m_presetName;
  
  float* p;


  int vw;
  int vh;

  float aspect;

  std::string presetURL;
  bool useWiiLight;
  
  void SetupPass1(const Pipeline &pipeline, const PipelineContext &pipelineContext);
  void Interpolation(const Pipeline &pipeline);
  void RenderItems(const Pipeline &pipeline, const PipelineContext &pipelineContext);
  void FinishPass1();
  void Pass2 (const Pipeline &pipeline, const PipelineContext &pipelineContext);
  void CompositeOutput(const Pipeline &pipeline, const PipelineContext &pipelineContext);

  inline static Point PerPixel(Point p, PerPixelContext &context)
  {
	  return currentPipe->PerPixel(p,context);
  }

  void rescale_per_pixel_matrices();

};

#endif
