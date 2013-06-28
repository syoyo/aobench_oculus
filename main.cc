#include <OVR.h>
#include "../Src/Util/Util_Render_Stereo.h" // relative to LibOVR/Include

#include <string>

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "trackball.h"

static float currQuat[4];
static float prevQuat[4];
static float viewOrg[3];
static float viewTarget[3];
static int mouseMoving;
static int mouseButton;     // 1 = left, 2 = middle, 3 = right
static int spinning;
static int mouseX;
static int mouseY;
static int width = 1280;
static int height = 800;
static int frameCounter = 0;
static float eyeOffset = 6.4; // heuristic

static void CheckGLErrors(std::string desc) {
  GLenum e = glGetError();
  if (e != GL_NO_ERROR) {
    fprintf(stderr, "OpenGL error in \"%s\": %d (%d)\n", desc.c_str(), e, e);
    exit(20);
  }
}

typedef struct
{
  GLuint  fboID;
  GLuint  renderToTexID;
  int     width, height;

  // AOBench pass
  GLuint  programAO;
  GLuint  vsAO; // vertex shader
  GLuint  fsAO; // fragment shader
  const char* vsAOSourceFile;
  const char* fsAOSourceFile;

  // Postprocess pass
  GLuint  program;
  GLuint  vs; // vertex shader
  GLuint  fs; // fragment shader

  const char* vsSourceFile;
  const char* fsSourceFile;

  //
  float distortionXCenterOffset;
  float distortionScale;
  float distortionK[4];
  float distortionCA[4];  // chromatic aberration

} RenderConfig;

RenderConfig gRenderConfig;

static bool
CompileShader(
  GLenum shaderType,  // GL_VERTEX_SHADER or GL_FRAGMENT_SHADER 
  GLuint& shader,
  const char* src)
{
  GLint val = 0;

  // free old shader/program
  if (shader != 0) glDeleteShader(shader);

  shader = glCreateShader(shaderType);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &val);
  if (val != GL_TRUE) {
    char log[4096];
    GLsizei msglen;
    glGetShaderInfoLog(shader, 4096, &msglen, log);
    printf("%s\n", log);
    assert(val == GL_TRUE && "failed to compile shader");
  }

  printf("Compile shader OK\n");
  return true;
}

static bool
LoadShader(
  GLenum shaderType,  // GL_VERTEX_SHADER or GL_FRAGMENT_SHADER 
  GLuint& shader,
  const char* shaderSourceFilename)
{
  GLint val = 0;

  // free old shader/program
  if (shader != 0) glDeleteShader(shader);

  static GLchar srcbuf[16384];
  FILE *fp = fopen(shaderSourceFilename, "rb");
  if (!fp) {
    fprintf(stderr, "failed to load shader: %s\n", shaderSourceFilename);
    return false;
  }
  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp);
  rewind(fp);
  len = fread(srcbuf, 1, len, fp);
  srcbuf[len] = 0;
  fclose(fp);

  static const GLchar *src = srcbuf;

  shader = glCreateShader(shaderType);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &val);
  if (val != GL_TRUE) {
    char log[4096];
    GLsizei msglen;
    glGetShaderInfoLog(shader, 4096, &msglen, log);
    printf("%s\n", log);
    assert(val == GL_TRUE && "failed to compile shader");
  }

  printf("Load shader [ %s ] OK\n", shaderSourceFilename);
  return true;
}

static bool
LinkShader(
  GLuint& prog,
  GLuint& vertShader,
  GLuint& fragShader)
{
  GLint val = 0;
  
  if (prog != 0) {
    glDeleteProgram(prog);
  }

  prog = glCreateProgram();

  glAttachShader(prog, vertShader);
  glAttachShader(prog, fragShader);
  glLinkProgram(prog);

  glGetProgramiv(prog, GL_LINK_STATUS, &val);
  assert(val == GL_TRUE && "failed to link shader");

  printf("Link shader OK\n");

  return true;
}

static bool
PrepareAOBenchShader(
  RenderConfig& config)
{

  bool ret;

  config.vs = 0;
  ret = LoadShader(GL_VERTEX_SHADER, config.vsAO, config.vsAOSourceFile);
  assert(ret);

  config.fs = 0;
  ret = LoadShader(GL_FRAGMENT_SHADER, config.fsAO, config.fsAOSourceFile);
  assert(ret);

  ret = LinkShader(config.programAO, config.vsAO, config.fsAO);
  assert(ret);

  return true;
}

static bool
PreparePostProcessShader(
  RenderConfig& config)
{

  bool ret;

  config.vs = 0;
  ret = LoadShader(GL_VERTEX_SHADER, config.vs, config.vsSourceFile);
  assert(ret);

  config.fs = 0;
  ret = LoadShader(GL_FRAGMENT_SHADER, config.fs, config.fsSourceFile);
  assert(ret);

  ret = LinkShader(config.program, config.vs, config.fs);
  assert(ret);

  return true;
}

static bool
GenRenderToTexture(
  RenderConfig& config)
{
  glGenTextures(1, &config.renderToTexID);
  CheckGLErrors("glGenTextures");

  glBindTexture(GL_TEXTURE_2D, config.renderToTexID);
  CheckGLErrors("glBindTextures");

  // Give an empty image to OpenGL
  glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA8, config.width, config.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  CheckGLErrors("glTexImage2D");

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glGenFramebuffersEXT(1, &config.fboID);
  CheckGLErrors("glGenFramebuffersEXT");
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, config.fboID);
  CheckGLErrors("glBindFramebufferEXT fbo");

  // Attach a texture to the FBO 
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, config.renderToTexID, 0);
  CheckGLErrors("glFramebufferTexture2DEXT");
   
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

  return true;
}

static bool
PostProcessRender(
  const RenderConfig& config,
  int   eyeType)
{
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
  glUseProgram(config.program);

  float w = 0.5f; // vp.w / windowW
  float h = 1.0f; // vp.h / windowH
  float x = 0.0f; // vp.x / windowW
  float y = 0.0f; // vp.h / windowH

  float xoffsetScale = 1.0f;
  int xoffset = 0;
  if (eyeType == 2) { // right
    x = 0.5f;
    xoffset = width / 2.0f;
    xoffsetScale = -1.0f; // negate
  }
  
  float as = (0.5*width) / (float)height;
  //printf("as = %f\n", as);

  float LensCenter[2];
  LensCenter[0] = x + (w + xoffsetScale * config.distortionXCenterOffset * 0.5f)*0.5f;
  LensCenter[1] = y + h*0.5f;

  float ScreenCenter[2];
  ScreenCenter[0] = x + w*0.5f;
  ScreenCenter[1] = y + h*0.5f;

  float scaleFactor = 1.0f / config.distortionScale;
  //printf("scaleFactor = %f\n", scaleFactor);

  float Scale[2];
  Scale[0] = (w/2) * scaleFactor;
  Scale[1] = (h/2) * scaleFactor * as;

  float ScaleIn[2];
  ScaleIn[0] = (2/w);
  ScaleIn[1] = (2/h) / as;
  //printf("scaleIn = %f, %f\n", ScaleIn[0], ScaleIn[1]);

  glUniform1i(glGetUniformLocation(config.program, "Texture0"), 0);
  CheckGLErrors("Texture0");

  glUniform2fv(glGetUniformLocation(config.program, "LensCenter"), 1, LensCenter);
  CheckGLErrors("LensCenter");
  glUniform2fv(glGetUniformLocation(config.program, "ScreenCenter"), 1, ScreenCenter);
  CheckGLErrors("ScreenCenter");
  glUniform2fv(glGetUniformLocation(config.program, "Scale"), 1, Scale);
  CheckGLErrors("Scale");
  glUniform2fv(glGetUniformLocation(config.program, "ScaleIn"), 1, ScaleIn);
  CheckGLErrors("ScaleIn");
  glUniform4fv(glGetUniformLocation(config.program, "HmdWarpParam"), 1, config.distortionK);
  CheckGLErrors("HmdWarpParam");
  glUniform4fv(glGetUniformLocation(config.program, "ChromAbParam"), 1, config.distortionCA);
  CheckGLErrors("ChromAbParam");

  //GLuint texID = glGetUniformLocation(config.program, "renderedTexture");
  float eye_offset_x = 0.0f;
  if (eyeType == 2) { // right
    eye_offset_x = 0.5f;
  }
  glUniform1f(glGetUniformLocation(config.program, "eye_offset_x"), eye_offset_x);
  CheckGLErrors("eye_offset_x");

  glViewport(xoffset, 0, width/2.0f, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1, 1, -1, 1, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, config.renderToTexID);
  CheckGLErrors("glBindTexture");
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glColor3f(0.0, 0.0, 1.0);
  glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, 1.0f, -0.5f); // Upper Left
      glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, 1.0f, -0.5f); // Upper Right
      glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,-1.0f, -0.5f); // Lower Right
      glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,-1.0f, -0.5f); // Lower Left
  glEnd();

  glFlush();
  glEnable(GL_DEPTH_TEST);

  return true;
}

static void
mouse(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        trackball(prevQuat, 0.0, 0.0, 0.0, 0.0);

        mouseMoving = 1;
        mouseButton = 1;
        mouseX = x;
        mouseY = y;
        spinning = 0;

    }

    if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        mouseMoving = 0;
    }
    
}

static void
motion(int x, int y)
{
    float w = 1;

    if (mouseMoving) {
        if (mouseButton == 1) {
            trackball(prevQuat,
                w * (2.0 * mouseX - width) / (float)width, 
                w * (height - 2.0 * mouseY) / (float)height, 
                w * (2.0 * x - width) / (float)width, 
                w * (height - 2.0 * y) / (float)height);
            add_quats(prevQuat, currQuat, currQuat);
        }
    }

    glutPostRedisplay();
}

static void
keyboard(unsigned char c, int x, int y)
{
    switch(c){
    case 27: // ESC
    case 'q':
        exit(EXIT_SUCCESS);
        break;
    case 'e':
        eyeOffset += 0.01;
        printf("eo = %f\n", eyeOffset);
        break;
    case 'w':
        eyeOffset -= 0.01;
        printf("eo = %f\n", eyeOffset);
        break;
    }

    glutPostRedisplay();
}

static void
idle()
{
  frameCounter++;
  glutPostRedisplay();
}

static void
RenderStereo(int eyeType)
{
  float mat[4][4];

  // eyeType : 0 = center, 1 = left, 2 = right
  
  //
  // Pass1. abench rendering. Render to texture.
  //
  
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, gRenderConfig.fboID);
  CheckGLErrors("glBindFramebufferEXT in display");

  // No depth buffer.
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
  CheckGLErrors("glFramebufferRenderbufferEXT");

  // Set the list of draw buffers.
  GLenum DrawBuffers[2] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers
  CheckGLErrors("glDrawBuffers");

  // Ensure framebuffer is done rendering.
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    assert(0);
  }
  CheckGLErrors("glCheckFramebufferStatus");
  
  glUseProgram(gRenderConfig.programAO);

  float resolution[2];
  resolution[0] = gRenderConfig.width;
  resolution[1] = gRenderConfig.height;
  glUniform2fv(glGetUniformLocation(gRenderConfig.programAO, "resolution"), 1, resolution);
  CheckGLErrors("resolution");

  float time = frameCounter * 0.01f;
  //printf("time = %f\n", time);
  glUniform1f(glGetUniformLocation(gRenderConfig.programAO, "time"), time);
  CheckGLErrors("time");

  float eyeOffsetX = eyeOffset;
  if (eyeType == 2) {
    eyeOffsetX = -eyeOffsetX;
  }

  glUniform1f(glGetUniformLocation(gRenderConfig.programAO, "eye_offset"), eyeOffsetX);
  CheckGLErrors("eye_offset");


  glViewport(0, 0, gRenderConfig.width, gRenderConfig.height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1, 1, -1, 1, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

  glDisable(GL_DEPTH_TEST);
  glColor3f(1.0, 1.0, 1.0);
  glBegin(GL_QUADS);
      glVertex3f(-1.0f, 1.0f, -0.5f); // Upper Left
      glVertex3f( 1.0f, 1.0f, -0.5f); // Upper Right
      glVertex3f( 1.0f,-1.0f, -0.5f); // Lower Right
      glVertex3f(-1.0f,-1.0f, -0.5f); // Lower Left
  glEnd();

  glEnable(GL_DEPTH_TEST);

  glFinish();
  CheckGLErrors("glFinish");

  // Unbind the FBO
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); 
  CheckGLErrors("Unbind");

  //
  // Pass2: Post process filter
  //
  PostProcessRender(gRenderConfig, eyeType);

}


static void
display()
{
  int left = 1;
  int right = 2;

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

  RenderStereo(left);
  RenderStereo(right);

	glutSwapBuffers();

}

int
main(
  int argc,
  char **argv)
{

  //
  // OVR
  //
  OVR::Util::Render::StereoConfig stereoConfig;
  stereoConfig.SetFullViewport(OVR::Util::Render::Viewport(0,0, width, height));
  stereoConfig.SetStereoMode(OVR::Util::Render::Stereo_LeftRight_Multipass);

  bool OculusFound = false;

  {
    OVR::Ptr<OVR::DeviceManager>  pDeviceManager;
    OVR::Ptr<OVR::HMDDevice>      pHMDDevice;
    OVR::Ptr<OVR::SensorDevice>   pSensorDevice;
    
    OVR::HMDInfo        hmd;

    // Initializes LibOVR. This LogMask_All enables maximum logging.
    // Custom allocator can also be specified here.
    OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));

    pDeviceManager = *OVR::DeviceManager::Create();
    pHMDDevice     = *pDeviceManager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();
    
    if (pHMDDevice) {

      pSensorDevice = *pHMDDevice->GetSensor();

      bool ret = pHMDDevice->GetDeviceInfo(&hmd);
      if (ret) {
        //hmd.DisplayDeviceName;
        //hmd.InterpupillaryDistance;
        //hmd.DistortionK[0];
        //hmd.DistortionK[1];
        //hmd.DistortionK[2];
        //hmd.DistortionK[3];

        // Configure proper Distortion Fit.
        // For 7" screen, fit to touch left side of the view, leaving a bit of invisible
        // screen on the top (saves on rendering cost).
        // For smaller screens (5.5"), fit to the top.
        if (hmd.HScreenSize > 0.0f)
        {
            if (hmd.HScreenSize > 0.140f) // 7"
                stereoConfig.SetDistortionFitPointVP(-1.0f, 0.0f);
            else
                stereoConfig.SetDistortionFitPointVP(0.0f, 1.0f);
        }

        stereoConfig.SetHMDInfo(hmd);

        OculusFound = true;
        fprintf(stderr, "Oculus Rift found! Cool.\n");
      }
    }

    if (!OculusFound) {
      fprintf(stderr, "Oculus Rift not found.\n");
    }
     
    OVR::SensorFusion sensorFusion;

    // No OVR functions involving memory are allowed after this.
    //while(1) {
    //  OVR::Quatf q = sensorFusion.GetOrientation();
    //}
  }

  OVR::Util::Render::StereoEyeParams param = stereoConfig.GetEyeRenderParams(OVR::Util::Render::StereoEye_Right);

  OVR::Util::Render::DistortionConfig distortionConfig = *(param.pDistortion);
  gRenderConfig.distortionXCenterOffset = distortionConfig.XCenterOffset;
  gRenderConfig.distortionK[0] = distortionConfig.K[0];
  gRenderConfig.distortionK[1] = distortionConfig.K[1];
  gRenderConfig.distortionK[2] = distortionConfig.K[2];
  gRenderConfig.distortionK[3] = distortionConfig.K[3];
  gRenderConfig.distortionScale = distortionConfig.Scale;
  gRenderConfig.distortionCA[0] = distortionConfig.ChromaticAberration[0];
  gRenderConfig.distortionCA[1] = distortionConfig.ChromaticAberration[1];
  gRenderConfig.distortionCA[2] = distortionConfig.ChromaticAberration[2];
  gRenderConfig.distortionCA[3] = distortionConfig.ChromaticAberration[3];

  printf("XCenterOffset: %f\n", gRenderConfig.distortionXCenterOffset);
  printf("Scale        : %f\n", gRenderConfig.distortionScale);
  printf("K: %f, %f, %f, %f\n",
    gRenderConfig.distortionK[0],
    gRenderConfig.distortionK[1],
    gRenderConfig.distortionK[2],
    gRenderConfig.distortionK[3]);
  printf("ChromaticAberration: %f, %f, %f, %f\n",
    gRenderConfig.distortionCA[0],
    gRenderConfig.distortionCA[1],
    gRenderConfig.distortionCA[2],
    gRenderConfig.distortionCA[3]);

  //
  // GL setup
  //

	glutInit(&argc, argv);
	glutInitWindowSize(width, height);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);

  //RenderPass1(gRenderConfig);

	glutCreateWindow(argv[0]);
	glutDisplayFunc(display);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  glutKeyboardFunc(keyboard);
  glutIdleFunc(idle);

  gRenderConfig.vsAOSourceFile = "aobench.vs";
  gRenderConfig.fsAOSourceFile = "aobench.fs";

  gRenderConfig.vsSourceFile = "postprocess.vs";
  gRenderConfig.fsSourceFile = "postprocess.fs";
  gRenderConfig.width    = 512;
  gRenderConfig.height   = 512;

  PrepareAOBenchShader(gRenderConfig);

  PreparePostProcessShader(gRenderConfig);
  GenRenderToTexture(gRenderConfig);

  glutFullScreen();

	glutMainLoop();

  //
  // OVR tear down
  //
  //OVR::System::Destroy();

  return 0;
}
