#include <math.h>

#ifdef PLATFORM_IOS
#define max(x, y) (x > y ? x : y)
#endif


#include "PVRShell.h"
#include "OGLES2Tools.h"

#define RTT_SIZE 128
#define SHADOW_MAP_SIZE 512
#define FLOOR_ALPHA 0.85f
#define ELEMENTS_IN_ARRAY(x) (sizeof(x) / sizeof(x[0]))

#define ASSERT(x) assert(x)

enum enumEFFECT
	{
	enumEFFECT_Model,
	enumEFFECT_Church,
	enumEFFECT_ScreenAlignedTex,
	enumEFFECT_Bloom1,
	enumEFFECT_BloomBlur,
	enumEFFECT_SimpleModel,
	enumEFFECT_ChurchRefl,
	enumEFFECT_MAX,
	};

enum enumTEXTURE
	{
	enumTEXTURE_Floor,
	enumTEXTURE_StatueNormals,
	enumTEXTURE_BloomMap,
	enumTEXTURE_ChurchWalls,
	enumTEXTURE_ChurchLightmap,
	enumTEXTURE_FloorLightmap,
	enumTEXTURE_MAX,
	};

enum enumMODEL
	{
	enumMODEL_Statue,
	enumMODEL_Floor,
	enumMODEL_Church,
	enumMODEL_MAX,
	};

enum enumFRAMEBUFFER
	{
	enumFB_1		= 0,
	enumFB_2		= 1,
	enumFB_MAX,
	};

const GLuint FLAG_VRT	= (1 << 1);
const GLuint FLAG_TEX0	= (1 << 2);
const GLuint FLAG_TEX1	= (1 << 3);
const GLuint FLAG_NRM	= (1 << 4);
const GLuint FLAG_TAN	= (1 << 5);
const GLuint FLAG_BIN	= (1 << 6);

// Utility function to strip the leading folders for iOS
const char* StripFolder(const char* c_pszFilename)
	{
#ifdef PLATFORM_IOS
	const char* c_pSlash = strrchr(c_pszFilename, '/');
	if(!c_pSlash)
		return c_pszFilename;
	else 
		return c_pSlash + 1;
#else
	return c_pszFilename;
#endif
	}

// ---------------------------------------------------------- RESOURCES
const char* c_pszTextures[] = 
	{
	"floor.pvr",			// enumTEXTURE_Floor
	"statuenormals.pvr",	// enumTEXTURE_StatueNormals
	"bloom_mapping.pvr",	// enumTEXTURE_BloomMap
	"church.pvr",			// enumTEXTURE_ChurchWalls
	"church-lightmap.pvr",	// enumTEXTURE_ChurchLightmap
	"floor-lightmap.pvr",	// enumTEXTURE_FloorLightmap
	};

const char c_szSceneFile[] = "statuescene.POD";

// ---------------------------------------------------------- SHADERS
struct GenericShader		// Base shader
	{
	GLuint uiID;
	};

// ------------------------------------- Statue shader
const char c_szModelShaderFSrc[]	= "GPUPrograms/StatueShader.fsh";
const char c_szModelShaderVSrc[]	= "GPUPrograms/StatueShader.vsh";
struct StatueShader : public GenericShader
	{
	GLuint uiMVP;
	GLuint uiModelView;
	GLuint uiLightPos;
	};

// ------------------------------------- Statue bloom shader 1
const char c_szBloom1ShaderFSrc[]	= "GPUPrograms/StatueBloom1.fsh";
const char c_szBloom1ShaderVSrc[]	= "GPUPrograms/StatueBloom1.vsh";
struct Bloom1Shader : public StatueShader
	{
	GLuint uiBloomMulti;
	};

// ------------------------------------- Church Shader
const char c_szChurchShaderFSrc[]	= "GPUPrograms/ChurchShader.fsh";
const char c_szChurchShaderVSrc[]	= "GPUPrograms/ChurchShader.vsh";
struct ChurchShader  : public GenericShader
	{
	GLuint uiModelView;
	GLuint uiProjection;
	GLuint uiTexProjection;
	GLuint uiAlpha;
	};
struct ChurchReflShader  : public GenericShader		// Uses the same source code as std. shader but with defines.
	{
	GLuint uiModelView;
	GLuint uiProjection;
	};
const char* const c_szChurchShaderDefs[] =
	{
	"USE_SHADOW_MAP",
	};

// ------------------------------------- Screen Aligned Texture
const char c_szSATextureFSrc[]	= "GPUPrograms/ScreenAlignedTexture.fsh";
const char c_szSATextureVSrc[]	= "GPUPrograms/ScreenAlignedTexture.vsh";
struct SATexShader  : public GenericShader
	{
	};

// ------------------------------------- Screen Aligned Texture
const char c_szBloomBlurFSrc[]	= "GPUPrograms/BloomBlur.fsh";
const char c_szBloomBlurVSrc[]	= "GPUPrograms/BloomBlur.vsh";
struct BloomBlurShader  : public GenericShader
	{
	GLuint uiTexelOffset;
	};
// ------------------------------------- Very simple vertex/frag shader
const char c_szSimpleFSrc[]	= "GPUPrograms/SimpleShader.fsh";
const char c_szSimpleVSrc[]	= "GPUPrograms/SimpleShader.vsh";
struct SimpleShader  : public GenericShader
	{
	GLuint uiMVP;
	};


enum enumATTRIBUTE
	{
	enumATTRIBUTE_POSITION,
	enumATTRIBUTE_TEXCOORD0,
	enumATTRIBUTE_NORMAL,
	enumATTRIBUTE_TANGENT,
	};

class MyPVRDemo : public PVRShell
	{
	private:
		// Models
		CPVRTModelPOD			m_Model;

		// Shaders
		StatueShader			m_StatueShader;
		ChurchShader			m_ChurchShader;
		SATexShader				m_SATexShader;
		Bloom1Shader			m_Bloom1Shader;
		BloomBlurShader			m_BloomBlurShader;
		SimpleShader			m_SimpleShader;
		ChurchReflShader		m_ChurchReflShader;

		// Textures
		GLuint					m_tex[enumTEXTURE_MAX];

		// Lights
		PVRTVec3				m_vLightPos;
		PVRTMat4				m_mxLightView;
		PVRTMat4				m_mxLightProj;
		PVRTMat4				m_mxLightBias;			// Bias matrix used to smooth out the shadow texture
		float					m_fLightAngle;

		// Scene matrices
		bool					m_bRotated;
		PVRTMat4				m_mxProjection;
		PVRTMat4				m_mxCam;

		float					m_fAngleY;
		float					m_fBloomMulti;
		float					m_fTexelOffset;

		PVRTVec4				m_bbStatueTL;
		PVRTVec4				m_bbStatueBR;

		// GL Handles
		GLuint					m_uiVertShader[enumEFFECT_MAX];
		GLuint					m_uiFragShader[enumEFFECT_MAX];
		GLuint					m_uiVBO[enumMODEL_MAX];
		GLuint					m_uiVBOIdx[enumMODEL_MAX];

		// FBO Handles
		GLint					m_nOrigFBO;
		GLuint					m_uiRTT[enumFB_MAX];		// Handle for a RTT texture
		GLuint					m_uiFBO[enumFB_MAX];		// Handle for FBO
		GLuint					m_uiFBODepth;				// Handle for depth buffer
		
		GLuint					m_uiShadowMapTex;			// Texture for the shadow map
		GLuint					m_uiShadowMapFBO;


		unsigned long			m_ulCurrTime;
		float					m_fDT;

	private:
		bool LoadTextures(CPVRTString* pErrorStr);
		bool LoadShaders(CPVRTString* pErrorStr);
		void LoadVBOs();
		bool CreateFBOs(CPVRTString* pErrorStr);

		void RenderStatue(const PVRTMat4& mxModel, const PVRTMat4& mxCam, const PVRTVec3& vLightPos, const StatueShader* pShader);
		void RenderCurch(const PVRTMat4& mxCam);
		void RenderScreenAlignedTexture(const PVRTVec2& vTL, const PVRTVec2& vBR, const PVRTVec2& vTTL, const PVRTVec2& vTBR);
		void RenderBloom(const PVRTMat4& mxModel, const PVRTMat4& mxCam, const PVRTVec3& vLightPos);
		void DrawMesh(int i32NodeIndex, GLuint uiFlags);

		void RenderShadowScene();

	public:
		virtual bool InitApplication();
		virtual bool InitView();
		virtual bool ReleaseView();
		virtual bool QuitApplication();
		virtual bool RenderScene();
	};

// ---------------------------------------------------------------
bool MyPVRDemo::LoadTextures(CPVRTString* const pErrorStr)
	{
	ASSERT(ELEMENTS_IN_ARRAY(c_pszTextures) == enumTEXTURE_MAX);

	// Load Textures from PVR files
	for(int i = 0; i < enumTEXTURE_MAX; ++i)
		{
		if(PVRTTextureLoadFromPVR(c_pszTextures[i], &m_tex[i]) != PVR_SUCCESS)
			{
			*pErrorStr = CPVRTString("ERROR: Could not load: ") + CPVRTString(c_pszTextures[i]);
			return false;
			}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}

	// Set some options
	glBindTexture(GL_TEXTURE_2D, m_tex[enumTEXTURE_BloomMap]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Allocate a texture for the RTT
	glGenTextures(enumFB_MAX, m_uiRTT);
	for(int i = 0; i < enumFB_MAX; i++)
		{
		glBindTexture(GL_TEXTURE_2D, m_uiRTT[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RTT_SIZE, RTT_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

	// Allocate a texture for the shadow map
	glGenTextures(1, &m_uiShadowMapTex);
	glBindTexture(GL_TEXTURE_2D, m_uiShadowMapTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return true;
	}

// ---------------------------------------------------------------
bool MyPVRDemo::LoadShaders(CPVRTString* pErrorStr)
	{
	// ---- Load Model Shader
		{
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szModelShaderVSrc), GL_VERTEX_SHADER, GL_SGX_BINARY_IMG, &m_uiVertShader[enumEFFECT_Model], pErrorStr) != PVR_SUCCESS)
			return false;
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szModelShaderFSrc), GL_FRAGMENT_SHADER, GL_SGX_BINARY_IMG, &m_uiFragShader[enumEFFECT_Model], pErrorStr) != PVR_SUCCESS)
			return false;

		const char* aszAttribs[] = { "inVertex", "inTexCoord", "inNormal", "inTangent" };
		if (PVRTCreateProgram(&m_StatueShader.uiID, m_uiVertShader[enumEFFECT_Model], m_uiFragShader[enumEFFECT_Model], aszAttribs, 3, pErrorStr) != PVR_SUCCESS)
			return false;

		// --- Get Uniform locations
		m_StatueShader.uiMVP			= glGetUniformLocation(m_StatueShader.uiID, "MVPMatrix");
		m_StatueShader.uiModelView		= glGetUniformLocation(m_StatueShader.uiID, "ModelView");
		m_StatueShader.uiLightPos		= glGetUniformLocation(m_StatueShader.uiID, "LightPosition");

		// --- Set some uniforms
		glUniform3f(glGetUniformLocation(m_StatueShader.uiID, "vDiffuse"), 0.5f, 0.5f, 0.5f);
		glUniform3f(glGetUniformLocation(m_StatueShader.uiID, "vSpecular"), 0.3f, 0.3f, 0.3f);
		glUniform1f(glGetUniformLocation(m_StatueShader.uiID, "fShininess"), 50.0f);
		glUniform1i(glGetUniformLocation(m_StatueShader.uiID, "sNormMap"), 0);
		}

	// ---- Load Bloom 1 shader. (Like Model Shader, but more optimized)
		{
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szBloom1ShaderVSrc), GL_VERTEX_SHADER, GL_SGX_BINARY_IMG, &m_uiVertShader[enumEFFECT_Bloom1], pErrorStr) != PVR_SUCCESS)
			return false;
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szBloom1ShaderFSrc), GL_FRAGMENT_SHADER, GL_SGX_BINARY_IMG, &m_uiFragShader[enumEFFECT_Bloom1], pErrorStr) != PVR_SUCCESS)
			return false;

		const char* aszAttribs[] = { "inVertex", "inTexCoord", "inNormal", "inTangent" };
		if (PVRTCreateProgram(&m_Bloom1Shader.uiID, m_uiVertShader[enumEFFECT_Bloom1], m_uiFragShader[enumEFFECT_Bloom1], aszAttribs, 3, pErrorStr) != PVR_SUCCESS)
			return false;

		// --- Get Uniform locations
		m_Bloom1Shader.uiMVP			= glGetUniformLocation(m_Bloom1Shader.uiID, "MVPMatrix");
		m_Bloom1Shader.uiModelView		= glGetUniformLocation(m_Bloom1Shader.uiID, "ModelView");
		m_Bloom1Shader.uiLightPos		= glGetUniformLocation(m_Bloom1Shader.uiID, "LightPosition");
		m_Bloom1Shader.uiBloomMulti		= glGetUniformLocation(m_Bloom1Shader.uiID, "fBloomMulti");

		// --- Set some uniforms
		glUniform1i(glGetUniformLocation(m_Bloom1Shader.uiID, "sNormMap"), 0);
		glUniform1i(glGetUniformLocation(m_Bloom1Shader.uiID, "sBloomMap"), 1);
		}

	// ---- Load Church Shader
		{
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szChurchShaderVSrc), GL_VERTEX_SHADER, GL_SGX_BINARY_IMG, &m_uiVertShader[enumEFFECT_Church], pErrorStr, 0, c_szChurchShaderDefs, 1) != PVR_SUCCESS)
			return false;
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szChurchShaderFSrc), GL_FRAGMENT_SHADER, GL_SGX_BINARY_IMG, &m_uiFragShader[enumEFFECT_Church], pErrorStr, 0, c_szChurchShaderDefs, 1) != PVR_SUCCESS)
			return false;

		const char* aszAttribs[] = { "inVertex", "inTexCoord0", "inTexCoord1" };
		if (PVRTCreateProgram(&m_ChurchShader.uiID, m_uiVertShader[enumEFFECT_Church], m_uiFragShader[enumEFFECT_Church], aszAttribs, 3, pErrorStr) != PVR_SUCCESS)
			return false;

		// --- Get Uniform locations
		m_ChurchShader.uiModelView				= glGetUniformLocation(m_ChurchShader.uiID, "mxModelView");
		m_ChurchShader.uiProjection				= glGetUniformLocation(m_ChurchShader.uiID, "mxProjection");
		m_ChurchShader.uiTexProjection			= glGetUniformLocation(m_ChurchShader.uiID, "mxTexProjection");
		m_ChurchShader.uiAlpha					= glGetUniformLocation(m_ChurchShader.uiID, "fAlpha");

		// --- Set some uniforms
		glUniform1i(glGetUniformLocation(m_ChurchShader.uiID, "sTexture"), 0);
		glUniform1i(glGetUniformLocation(m_ChurchShader.uiID, "sShadow"), 1);
		glUniform1i(glGetUniformLocation(m_ChurchShader.uiID, "sLightmap"), 2);
		}

	// ---- Load Church Reflection shader
		{
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szChurchShaderVSrc), GL_VERTEX_SHADER, GL_SGX_BINARY_IMG, &m_uiVertShader[enumEFFECT_ChurchRefl], pErrorStr) != PVR_SUCCESS)
			return false;
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szChurchShaderFSrc), GL_FRAGMENT_SHADER, GL_SGX_BINARY_IMG, &m_uiFragShader[enumEFFECT_ChurchRefl], pErrorStr) != PVR_SUCCESS)
			return false;

		const char* aszAttribs[] = { "inVertex", "inTexCoord0", "inTexCoord1" };
		if (PVRTCreateProgram(&m_ChurchReflShader.uiID, m_uiVertShader[enumEFFECT_ChurchRefl], m_uiFragShader[enumEFFECT_ChurchRefl], aszAttribs, 3, pErrorStr) != PVR_SUCCESS)
			return false;

		// --- Get Uniform locations
		m_ChurchReflShader.uiModelView				= glGetUniformLocation(m_ChurchReflShader.uiID, "mxModelView");
		m_ChurchReflShader.uiProjection				= glGetUniformLocation(m_ChurchReflShader.uiID, "mxProjection");

		// --- Set some uniforms
		glUniform1i(glGetUniformLocation(m_ChurchReflShader.uiID, "sTexture"), 0);
		glUniform1i(glGetUniformLocation(m_ChurchReflShader.uiID, "sLightmap"), 2);
		}

	// ---- Load Screen Aligned Texture shader
		{
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szSATextureVSrc), GL_VERTEX_SHADER, GL_SGX_BINARY_IMG, &m_uiVertShader[enumEFFECT_ScreenAlignedTex], pErrorStr) != PVR_SUCCESS)
			return false;
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szSATextureFSrc), GL_FRAGMENT_SHADER, GL_SGX_BINARY_IMG, &m_uiFragShader[enumEFFECT_ScreenAlignedTex], pErrorStr) != PVR_SUCCESS)
			return false;

		const char* aszAttribs[] = { "inVertex", "inTexCoord" };
		if (PVRTCreateProgram(&m_SATexShader.uiID, m_uiVertShader[enumEFFECT_ScreenAlignedTex], m_uiFragShader[enumEFFECT_ScreenAlignedTex], aszAttribs, 2, pErrorStr) != PVR_SUCCESS)
			return false;

		// --- Set some uniforms
		glUniform1i(glGetUniformLocation(m_SATexShader.uiID, "sTexture"), 0);
		}

	// ---- Load Blur shader
		{
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szBloomBlurVSrc), GL_VERTEX_SHADER, GL_SGX_BINARY_IMG, &m_uiVertShader[enumEFFECT_BloomBlur], pErrorStr) != PVR_SUCCESS)
			return false;
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szBloomBlurFSrc), GL_FRAGMENT_SHADER, GL_SGX_BINARY_IMG, &m_uiFragShader[enumEFFECT_BloomBlur], pErrorStr) != PVR_SUCCESS)
			return false;

		const char* aszAttribs[] = { "inVertex", "inTexCoord" };
		if (PVRTCreateProgram(&m_BloomBlurShader.uiID, m_uiVertShader[enumEFFECT_BloomBlur], m_uiFragShader[enumEFFECT_BloomBlur], aszAttribs, 2, pErrorStr) != PVR_SUCCESS)
			return false;

		m_BloomBlurShader.uiTexelOffset				= glGetUniformLocation(m_BloomBlurShader.uiID, "vTexelOffset");

		// --- Set some uniforms
		glUniform1i(glGetUniformLocation(m_BloomBlurShader.uiID, "sTexture"), 0);
		}


	// ---- Load a simple model shader
		{
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szSimpleVSrc), GL_VERTEX_SHADER, GL_SGX_BINARY_IMG, &m_uiVertShader[enumEFFECT_SimpleModel], pErrorStr) != PVR_SUCCESS)
			return false;
		if (PVRTShaderLoadFromFile(NULL, StripFolder(c_szSimpleFSrc), GL_FRAGMENT_SHADER, GL_SGX_BINARY_IMG, &m_uiFragShader[enumEFFECT_SimpleModel], pErrorStr) != PVR_SUCCESS)
			return false;

		const char* aszAttribs[] = { "inVertex" };
		if (PVRTCreateProgram(&m_SimpleShader.uiID, m_uiVertShader[enumEFFECT_SimpleModel], m_uiFragShader[enumEFFECT_SimpleModel], aszAttribs, 1, pErrorStr) != PVR_SUCCESS)
			return false;

		m_SimpleShader.uiMVP				= glGetUniformLocation(m_SimpleShader.uiID, "mxMVP");
		}

	return true;
	}

// ---------------------------------------------------------------
void MyPVRDemo::LoadVBOs()
	{
	glGenBuffers(enumMODEL_MAX, m_uiVBO);
	glGenBuffers(enumMODEL_MAX, m_uiVBOIdx);

	for (unsigned int i = 0; i < enumMODEL_MAX; ++i)
		{
		SPODMesh& Mesh = m_Model.pMesh[i];
		unsigned int uiSize = Mesh.nNumVertex * Mesh.sVertex.nStride;
		glBindBuffer(GL_ARRAY_BUFFER, m_uiVBO[i]);
		glBufferData(GL_ARRAY_BUFFER, uiSize, Mesh.pInterleaved, GL_STATIC_DRAW);

		uiSize = PVRTModelPODCountIndices(Mesh) * sizeof(GLshort);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uiVBOIdx[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, uiSize, Mesh.sFaces.pData, GL_STATIC_DRAW);
		}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

// ---------------------------------------------------------------
bool MyPVRDemo::CreateFBOs(CPVRTString* pErrorStr)
	{
	// Get the original FBO ID
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_nOrigFBO);
	
	// --- Create 2 FBOs
	glGenFramebuffers(enumFB_MAX, m_uiFBO);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// --- Create a render depth buffer
	glGenRenderbuffers(1, &m_uiFBODepth);
	glBindRenderbuffer(GL_RENDERBUFFER, m_uiFBODepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, RTT_SIZE, RTT_SIZE);

	for(int i = 0; i < enumFB_MAX; i++)
		{
		glBindFramebuffer(GL_FRAMEBUFFER, m_uiFBO[i]);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_uiRTT[i], 0);

		// Attach depth buffer to the first frame buffer
		if(i == 0)
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_uiFBODepth);

		if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			{
			*pErrorStr = "ERROR: Could not create framebuffer object";
			return false;
			}
		}

	// --- Create a framebuffer for the shadow map
	glGenFramebuffers(1, &m_uiShadowMapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_uiShadowMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_uiShadowMapTex, 0);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
		*pErrorStr = "ERROR: Could not create framebuffer object";
		return false;
		}


	// --- Done with FBO, so bind the original frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, m_nOrigFBO);
	return true;
	}

// ---------------------------------------------------------------
bool MyPVRDemo::InitApplication()
	{
#ifdef PLATFORM_IOS
	CPVRTResourceFile::SetReadPath((char*)PVRShellGet(prefReadPath));
#endif
	
	if (m_Model.ReadFromFile(c_szSceneFile) != PVR_SUCCESS)
		{
		PVRShellSet(prefExitMessage, "ERROR: Couldn't load the .pod file\n");
		return false;
		}

	assert(enumMODEL_MAX == m_Model.nNumMesh);

	// Calculate a bounding box around the statue that we can use later on
	SPODMesh* pMesh = &m_Model.pMesh[m_Model.pNode[enumMODEL_Statue].nIdx];
	PVRTBOUNDINGBOX bb;
	PVRTBoundingBoxComputeInterleaved(&bb, pMesh->pInterleaved, pMesh->nNumVertex, 0, pMesh->sVertex.nStride);

	// We need to calculate the longest possible length for our bounding box on the X and Z axis (as we're rotating around Y).
	float fLen[4];
	fLen[0] = sqrt(bb.Point[7].x * bb.Point[7].x + bb.Point[7].z * bb.Point[7].z);	// FR quadrant
	fLen[1] = sqrt(bb.Point[3].x * bb.Point[3].x + bb.Point[3].z * bb.Point[3].z);	// FL quadrant
	fLen[2] = sqrt(bb.Point[6].x * bb.Point[6].x + bb.Point[6].z * bb.Point[6].z);	// BR quadrant
	fLen[3] = sqrt(bb.Point[2].x * bb.Point[2].x + bb.Point[2].z * bb.Point[2].z);	// BL quadrant
	
	float fLongest = max(fLen[0], fLen[1]);
	fLongest = max(fLongest, fLen[2]);
	fLongest = max(fLongest, fLen[3]);
		
	// Calculate the top left and bottom right of the bounding box in 2D space
	m_bbStatueTL = PVRTVec4(-fLongest, bb.Point[3].y, bb.Point[3].z, 1.0f);
	m_bbStatueBR = PVRTVec4( fLongest, bb.Point[5].y, bb.Point[5].z, 1.0f);

	// Some nice variables
	m_fAngleY = 0.0f;
	m_fBloomMulti = 0.3f;
	m_ulCurrTime = 0;
	m_fLightAngle = PVRT_PI / 8;	// Offset by 22.5degrees to begin with, so we see the shadow slightly offset from behind the model.

	m_fTexelOffset = 1.0f / RTT_SIZE;
	float w1 = 0.0555555f;
	float w2 = 0.2777777f;
	float intraTexelOffset = (w2 / (w1 + w2)) * m_fTexelOffset;		// Weight between 2 texels so that hardware filtering will
																	// interpolate the texels for us. Effectively reducing a 5x5 kernel
																	// to a 3x3.
	m_fTexelOffset += intraTexelOffset;

	return true;
	}

// ---------------------------------------------------------------
bool MyPVRDemo::QuitApplication()
	{
	m_Model.Destroy();
    return true;
	}

// ---------------------------------------------------------------
bool MyPVRDemo::InitView()
	{
	CPVRTString ErrorStr;

	LoadVBOs();
	bool bResult = true;
	bResult &= LoadTextures(&ErrorStr);
	bResult &= LoadShaders(&ErrorStr);
	bResult &= CreateFBOs(&ErrorStr);
	
	if(!bResult)
		{
		PVRShellSet(prefExitMessage, ErrorStr.c_str());
		return false;
		}

	m_bRotated = PVRShellGet(prefIsRotated) && PVRShellGet(prefFullScreen);

	// --- Set up light position, projection and view
	m_vLightPos   = PVRTVec3(0, 125, 200);
	m_mxLightProj = PVRTMat4::PerspectiveFovRH(PVRT_PI / 4, 1.0f, 10.0f, 1000.0f, PVRTMat4::OGL, m_bRotated);
	m_mxLightView = PVRTMat4::LookAtRH(m_vLightPos, PVRTVec3(0,25,0), PVRTVec3(0,1,0));
	m_mxLightBias = PVRTMat4(0.5f, 0.0f, 0.0f, 0.0f,
							 0.0f, 0.5f, 0.0f, 0.0f,
							 0.0f, 0.0f, 0.5f, 0.0f,
							 0.5f, 0.5f, 0.5f, 1.0f);

	// --- Set up Camera projection and view
	float fAspect = PVRShellGet(prefWidth) / (float)PVRShellGet(prefHeight);
	m_mxProjection = PVRTMat4::PerspectiveFovFloatDepthRH(0.75f, fAspect, 10.0f, PVRTMat4::OGL, m_bRotated);
	m_mxCam = PVRTMat4::LookAtRH(PVRTVec3(0, 55, 150), PVRTVec3(0, 35, 0), PVRTVec3(0, 1, 0));

	// --- Set GL states
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GEQUAL);
	glClearDepthf(0.0f);
	glClearColor(0,0,0,1);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	return true;
	}

// ---------------------------------------------------------------
bool MyPVRDemo::ReleaseView()
	{
	glDeleteTextures(enumTEXTURE_MAX, m_tex);

	// --- Delete program and shader objects
	glDeleteProgram(m_StatueShader.uiID);
	for(int i = 0; i < enumEFFECT_MAX; ++i)
		{	
		glDeleteShader(m_uiVertShader[i]);
		glDeleteShader(m_uiFragShader[i]);
		}

	// --- Delete buffer objects
	glDeleteBuffers(enumMODEL_MAX, m_uiVBO);
	glDeleteBuffers(enumMODEL_MAX, m_uiVBOIdx);

	// --- Delete FBO
	glDeleteFramebuffers(enumFB_MAX, m_uiFBO);
	glDeleteRenderbuffers(1, &m_uiFBODepth);

	return true;
	}

// ---------------------------------------------------------------
bool MyPVRDemo::RenderScene()
	{
	// --- Work out DT
	unsigned long ulPrevTime = m_ulCurrTime;
	m_ulCurrTime = PVRShellGetTime();
	m_fDT = ((float)m_ulCurrTime - (float)ulPrevTime) * 0.001f;

	// Calculate a new light matrix
	PVRTVec3 vLightPos = PVRTVec4(m_vLightPos, 1.0f) * PVRTMat4::RotationY(m_fLightAngle);
	m_mxLightView = PVRTMat4::LookAtRH(vLightPos, PVRTVec3(0,25,0), PVRTVec3(0,1,0));

	// --- Render the scene from the light's POV
	RenderShadowScene();

	// --- Clear buffers
	glViewport(0, 0, PVRShellGet(prefWidth), PVRShellGet(prefHeight));
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	PVRTMat4 mxCam = m_mxCam * PVRTMat4::RotationY(m_fAngleY);
	PVRTMat4 mxModel = PVRTMat4::Identity();

	// --- Draw the Statue
	glUseProgram(m_StatueShader.uiID);
	glBindTexture(GL_TEXTURE_2D, m_tex[enumTEXTURE_StatueNormals]);
	RenderStatue(mxModel, mxCam, vLightPos, &m_StatueShader);

	// --- Draw the Statue reflected
	glCullFace(GL_FRONT);
	PVRTMat4 mxModelRefl = PVRTMat4::Scale(1,-1,1) * mxModel;
	RenderStatue(mxModelRefl, mxCam, vLightPos, &m_StatueShader);
	glCullFace(GL_BACK);

	// --- Draw the Floor (with shadow)
	RenderCurch(mxCam);

	// --- Render the bloom effect
	RenderBloom(mxModel, mxCam, vLightPos);

	// --- Increment the camera angle
	m_fAngleY += 0.5f * m_fDT;

	// --- Increment the light angle
	m_fLightAngle += 0.5f * m_fDT;
	return true;
	}

// ---------------------------------------------------------------
void MyPVRDemo::RenderShadowScene()
	{
	// --- Bind the shadow map FBO
	glBindFramebuffer(GL_FRAMEBUFFER, m_uiShadowMapFBO);
	glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);			// Turn off colour writing

	glUseProgram(m_SimpleShader.uiID);
	// Create MVP using the light's matrix properties
	PVRTMat4 mxMVP = m_mxLightProj * m_mxLightView * PVRTMat4::Identity();
	glUniformMatrix4fv(m_SimpleShader.uiMVP, 1, GL_FALSE, mxMVP.ptr());
	DrawMesh(enumMODEL_Statue, FLAG_VRT);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);				// We can turn colour writing back on.

	glBindFramebuffer(GL_FRAMEBUFFER, m_nOrigFBO);		// Done. Use the original framebuffer.
	glViewport(0, 0, PVRShellGet(prefWidth), PVRShellGet(prefHeight));
	}

// ---------------------------------------------------------------
void MyPVRDemo::RenderBloom(const PVRTMat4& mxModel, const PVRTMat4& mxCam, const PVRTVec3& vLightPos)
	{
	// --- Bind an empty frame buffer.
	glBindFramebuffer(GL_FRAMEBUFFER, m_uiFBO[enumFB_1]);
	glViewport(0, 0, RTT_SIZE, RTT_SIZE);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// --- Render the statue with Bloom Stage 1 Shader. This basically renders the statue with the normal map
	// and uses a texture lookup to effectively high-pass filter the resultant output.
	glUseProgram(m_Bloom1Shader.uiID);
	glUniform1f(m_Bloom1Shader.uiBloomMulti, m_fBloomMulti);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_tex[enumTEXTURE_StatueNormals]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_tex[enumTEXTURE_BloomMap]);
	RenderStatue(mxModel, mxCam, vLightPos, &m_Bloom1Shader);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	// --- BLUR PASSES
	glUseProgram(m_BloomBlurShader.uiID);

	// Horizontal blur
	glBindFramebuffer(GL_FRAMEBUFFER, m_uiFBO[enumFB_2]);
	glViewport(0, 0, RTT_SIZE, RTT_SIZE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, m_uiRTT[enumFB_1]);

	glUniform2f(m_BloomBlurShader.uiTexelOffset, m_fTexelOffset, 0.0f);
	RenderScreenAlignedTexture(PVRTVec2(-1.0f, 1.0f), PVRTVec2(1.0f, -1.0f), PVRTVec2(0.0f, 1.0f), PVRTVec2(1.0f, 0.0f));

	// Vertical blur
	glBindFramebuffer(GL_FRAMEBUFFER, m_uiFBO[enumFB_1]);
	glViewport(0, 0, RTT_SIZE, RTT_SIZE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, m_uiRTT[enumFB_2]);

	glUniform2f(m_BloomBlurShader.uiTexelOffset, 0.0f, m_fTexelOffset);
	RenderScreenAlignedTexture(PVRTVec2(-1.0f, 1.0f), PVRTVec2(1.0f, -1.0f), PVRTVec2(0.0f, 1.0f), PVRTVec2(1.0f, 0.0f));


	// --- OVERLAY PASS
	glBindFramebuffer(GL_FRAMEBUFFER, m_nOrigFBO);		// Done. Use the original framebuffer.
	glViewport(0, 0, PVRShellGet(prefWidth), PVRShellGet(prefHeight));

	// --- Render the texture to a screen aligned quad over the original mode. NOTE: We should use scissor or bounding rect instead.
	glUseProgram(m_SATexShader.uiID);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);				// Additive blending
	glBindTexture(GL_TEXTURE_2D, m_uiRTT[enumFB_1]);

	// This needs to be recalculated whenever either of the following change:
	PVRTMat4 mxMVP = m_mxProjection * m_mxCam * mxModel;
	PVRTVec4 vTL = mxMVP * m_bbStatueTL;
	PVRTVec4 vBR = mxMVP * m_bbStatueBR;
	vTL /= vTL.w;
	vBR /= vBR.w;

	if(m_bRotated)
		{
		float fTmp = vBR.y;			// Swap coordinates around if the screen is rotated.
		vBR.y = vTL.y;
		vTL.y = fTmp;
		}
	PVRTVec2 vTTL(vTL.x * 0.5f + 0.5f, vTL.y * 0.5f + 0.5f);
	PVRTVec2 vTBR(vBR.x * 0.5f + 0.5f, vBR.y * 0.5f + 0.5f);
	
	RenderScreenAlignedTexture(PVRTVec2(vTL.x, vTL.y), PVRTVec2(vBR.x, vBR.y), vTTL, vTBR);
	glDisable(GL_BLEND);
	}

// ---------------------------------------------------------------
void MyPVRDemo::RenderStatue(const PVRTMat4& mxModel, const PVRTMat4& mxCam, const PVRTVec3& vLightPos, const StatueShader* pShader)
	{
	PVRTMat4 mxModelView = mxCam * mxModel;
	PVRTMat4 mxMVP = m_mxProjection * mxModelView;
	PVRTVec3 vLightPosModel = vLightPos;		// Light position in World space
	glUniform3fv(pShader->uiLightPos, 1, vLightPosModel.ptr());
	glUniformMatrix4fv(pShader->uiMVP, 1, GL_FALSE, mxMVP.ptr());
	glUniformMatrix4fv(pShader->uiModelView, 1, GL_FALSE, mxModelView.ptr());
	DrawMesh(enumMODEL_Statue, FLAG_VRT | FLAG_TEX0 | FLAG_NRM | FLAG_TAN);
	}

// ---------------------------------------------------------------
void MyPVRDemo::RenderCurch(const PVRTMat4& mxCam)
	{
	PVRTMat4 mxModel = PVRTMat4::Identity();
	PVRTMat4 mxModelView = mxCam * mxModel;
	PVRTMat4 mxTexProj = m_mxLightBias * m_mxLightProj * m_mxLightView * mxCam.inverse();

	// --- Draw the floor reflected first, so we don't have to swap between GPU programs
	glUseProgram(m_ChurchReflShader.uiID);
	// Base map
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_tex[enumTEXTURE_ChurchWalls]);
	// Light map
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_tex[enumTEXTURE_ChurchLightmap]);

	glCullFace(GL_FRONT);
	PVRTMat4 mxReflChurchView = mxCam * PVRTMat4::Scale(1, -1, 1);
	glUniformMatrix4fv(m_ChurchReflShader.uiProjection, 1, GL_FALSE, m_mxProjection.ptr());
	glUniformMatrix4fv(m_ChurchReflShader.uiModelView, 1, GL_FALSE, mxReflChurchView.ptr());	// Reflected ModelView matrix
	DrawMesh(enumMODEL_Church, FLAG_VRT | FLAG_TEX0 | FLAG_TEX1);
	glCullFace(GL_BACK);

	// --- Activate the Church shader which utilises the Shadow Map.
	glUseProgram(m_ChurchShader.uiID);
	
	
	// --- Use the Shadow Map texture in texture unit 1
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_uiShadowMapTex);
	
	// --- Upload projection matrices
	glUniformMatrix4fv(m_ChurchShader.uiProjection, 1, GL_FALSE, m_mxProjection.ptr());	
	glUniformMatrix4fv(m_ChurchShader.uiTexProjection, 1, GL_FALSE, mxTexProj.ptr());
	glUniform1f(m_ChurchShader.uiAlpha, 1.0f);	// Set no alpha while we draw the walls.

	// --- Draw church walls	 (Textures are already bound)
	// Draw the walls as normal
	glUniformMatrix4fv(m_ChurchShader.uiModelView, 1, GL_FALSE, mxModelView.ptr());		// Standard ModelView matrix
	DrawMesh(enumMODEL_Church, FLAG_VRT | FLAG_TEX0 | FLAG_TEX1);

	// --- Draw floor
	glEnable(GL_BLEND);
	// Base map
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_tex[enumTEXTURE_Floor]);
	// Light map
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_tex[enumTEXTURE_FloorLightmap]);
	
	// Draw the floor
	glUniform1f(m_ChurchShader.uiAlpha, FLOOR_ALPHA);
	glUniformMatrix4fv(m_ChurchShader.uiModelView, 1, GL_FALSE, mxModelView.ptr());		// Standard ModelView matrix
	DrawMesh(enumMODEL_Floor, FLAG_VRT | FLAG_TEX0 | FLAG_TEX1);

	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
	}

// ---------------------------------------------------------------
void MyPVRDemo::RenderScreenAlignedTexture(const PVRTVec2& vTL, const PVRTVec2& vBR, const PVRTVec2& vTTL, const PVRTVec2& vTBR)
	{
	glDisable(GL_DEPTH_TEST);
	
	glEnableVertexAttribArray(enumATTRIBUTE_POSITION);
	glEnableVertexAttribArray(enumATTRIBUTE_TEXCOORD0);

	const float c_fVertex[] = {	vTL.x, vBR.y,		// Bottom Left
								vBR.x, vBR.y,		// Bottom Right
								vTL.x, vTL.y,		// Top Left
								vBR.x, vTL.y };		// Top Right
								
	const float c_fUVs[]	= { 
								vTTL.x, vTBR.y,		// Bottom Left
								vTBR.x, vTBR.y,		// Bottom Right
								vTTL.x, vTTL.y,		// Top Left
								vTBR.x, vTTL.y };	// Top Right

	glVertexAttribPointer(enumATTRIBUTE_POSITION, 2, GL_FLOAT, GL_FALSE, 0, c_fVertex);
	glVertexAttribPointer(enumATTRIBUTE_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, 0, c_fUVs);	

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(enumATTRIBUTE_POSITION);
	glDisableVertexAttribArray(enumATTRIBUTE_TEXCOORD0);
	
	glEnable(GL_DEPTH_TEST);
	}

// ---------------------------------------------------------------
void MyPVRDemo::DrawMesh(int nModelIdx, GLuint uiFlags)
	{
	int nMeshIdx = m_Model.pNode[nModelIdx].nIdx;
	SPODMesh* pMesh = &m_Model.pMesh[nMeshIdx];

	glBindBuffer(GL_ARRAY_BUFFER, m_uiVBO[nMeshIdx]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uiVBOIdx[nMeshIdx]);

	if(uiFlags & FLAG_VRT)	glEnableVertexAttribArray(enumATTRIBUTE_POSITION);
	if(uiFlags & FLAG_NRM)	glEnableVertexAttribArray(enumATTRIBUTE_NORMAL);
	if(uiFlags & FLAG_TEX0)	glEnableVertexAttribArray(enumATTRIBUTE_TEXCOORD0);
	if(uiFlags & FLAG_TEX1)	glEnableVertexAttribArray(enumATTRIBUTE_TEXCOORD0 + 1);
	if(uiFlags & FLAG_TAN)	glEnableVertexAttribArray(enumATTRIBUTE_TANGENT);	

	if(uiFlags & FLAG_VRT)	glVertexAttribPointer(enumATTRIBUTE_POSITION, 3, GL_FLOAT, GL_FALSE, pMesh->sVertex.nStride, pMesh->sVertex.pData);
	if(uiFlags & FLAG_NRM)	glVertexAttribPointer(enumATTRIBUTE_NORMAL, 3, GL_FLOAT, GL_FALSE, pMesh->sNormals.nStride, pMesh->sNormals.pData);
	if(uiFlags & FLAG_TEX0)	glVertexAttribPointer(enumATTRIBUTE_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, pMesh->psUVW[0].nStride, pMesh->psUVW[0].pData);
	if(uiFlags & FLAG_TEX1)	glVertexAttribPointer(enumATTRIBUTE_TEXCOORD0 + 1, 2, GL_FLOAT, GL_FALSE, pMesh->psUVW[1].nStride, pMesh->psUVW[1].pData);
	if(uiFlags & FLAG_TAN)	glVertexAttribPointer(enumATTRIBUTE_TANGENT, 3, GL_FLOAT, GL_FALSE, pMesh->sTangents.nStride, pMesh->sTangents.pData);

	glDrawElements(GL_TRIANGLES, pMesh->nNumFaces*3, GL_UNSIGNED_SHORT, 0);

	if(uiFlags & FLAG_VRT)	glDisableVertexAttribArray(enumATTRIBUTE_POSITION);
	if(uiFlags & FLAG_NRM)	glDisableVertexAttribArray(enumATTRIBUTE_NORMAL);
	if(uiFlags & FLAG_TEX0)	glDisableVertexAttribArray(enumATTRIBUTE_TEXCOORD0);
	if(uiFlags & FLAG_TEX1)	glDisableVertexAttribArray(enumATTRIBUTE_TEXCOORD0 + 1);
	if(uiFlags & FLAG_TAN)	glDisableVertexAttribArray(enumATTRIBUTE_TANGENT);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

// ---------------------------------------------------------------
PVRShell* NewDemo()
	{
	return new MyPVRDemo();
	}


