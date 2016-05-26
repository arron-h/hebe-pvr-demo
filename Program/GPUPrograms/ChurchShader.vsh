attribute highp		vec3	inPosition;
attribute mediump	vec2	inTexCoord0;
attribute mediump	vec2	inTexCoord1;

uniform highp	mat4	mxModelView;
uniform highp	mat4	mxProjection;
#ifdef USE_SHADOW_MAP
	uniform highp	mat4	mxTexProjection;
#endif

varying mediump	vec2	vTexCoord0;
varying mediump	vec2	vTexCoord1;
#ifdef USE_SHADOW_MAP
	varying highp	vec4	vProjCoord;
#endif

void main()
	{
	highp vec4 vModelView = mxModelView * vec4(inPosition, 1.0);
	gl_Position = mxProjection * vModelView;
#ifdef USE_SHADOW_MAP
	vProjCoord  = mxTexProjection * vModelView;
#endif
	
	vTexCoord0 = inTexCoord0;
	vTexCoord1 = inTexCoord1;
	}