#ifdef USE_SHADOW_MAP
	uniform sampler2D		sShadow;
#endif
uniform sampler2D		sTexture;
uniform sampler2D		sLightmap;
#ifdef USE_SHADOW_MAP
	uniform lowp float		fAlpha;
#endif

varying mediump	vec2	vTexCoord0;
varying mediump	vec2	vTexCoord1;
#ifdef USE_SHADOW_MAP
	varying highp	vec4	vProjCoord;
#endif

void main()
	{
#ifdef USE_SHADOW_MAP
	highp float fZComp = (vProjCoord.z / vProjCoord.w) - 0.03;		// Subtract an offset for error.
	highp vec4 vDepth = texture2DProj(sShadow, vProjCoord);
	
	mediump float fFragVal = max((1.0 - vDepth.r), 0.5);			// Use depth map so we can take advantage of linear filtering.
	lowp vec3 vFragCol = texture2D(sTexture, vTexCoord0).rgb * texture2D(sLightmap, vTexCoord1).rgb * fFragVal;
	
	gl_FragColor = vec4(vFragCol, fAlpha);
#else
	lowp vec3 vFragCol = texture2D(sTexture, vTexCoord0).rgb * texture2D(sLightmap, vTexCoord1).rgb;	
	gl_FragColor = vec4(vFragCol, 1.0);
#endif
	}