uniform sampler2D		sNormMap;
uniform sampler2D		sBloomMap;
uniform lowp float		fBloomMulti;

varying highp   vec3  L;
varying mediump vec2  TexCoord;

void main()
	{
	highp vec3 normal = texture2D(sNormMap, TexCoord).rgb * 2.0 - 1.0;		// Get normal and expand from 0.0 > 1.0 to -1.0 > 1.0
	normal = normalize(normal);		// Looks really shitty without this
	highp vec3 vL = normalize(L);	// Need to normalise due to interpolation
	
	lowp float NdotL = max(dot(normal, vL), 0.0);
	gl_FragColor = texture2D(sBloomMap, vec2(NdotL, 0.0)) * fBloomMulti;
	}
