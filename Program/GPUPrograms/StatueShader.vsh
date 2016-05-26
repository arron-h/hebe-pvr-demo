attribute highp vec3  inVertex; 
attribute highp vec3  inNormal;
attribute highp vec2  inTexCoord;
attribute highp vec3  inTangent;

uniform highp mat4  MVPMatrix;
uniform highp mat4  ModelView;
uniform highp vec3  LightPosition;

varying mediump vec2  TexCoord;
varying highp   vec3  L;
varying highp   vec3  vHalfVector;

void main()
	{
	gl_Position = MVPMatrix * vec4(inVertex, 1.0);
	
	highp vec3 ecPosition = vec3(ModelView * vec4(inVertex, 1.0));
	highp vec3 EyeDir     = -normalize(ecPosition);
	highp vec3 LightDir	  = normalize(LightPosition - inVertex);
	
	highp vec3 bitangent = cross(inNormal, inTangent);
	highp mat3 mxTangentSpace = mat3(inTangent, bitangent, inNormal);
	
	L = LightDir * mxTangentSpace;
	TexCoord = inTexCoord;
	vHalfVector = normalize(L + EyeDir);
	}