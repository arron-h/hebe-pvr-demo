uniform sampler2D		sNormMap;
uniform lowp vec3		vSpecular;
uniform lowp vec3		vDiffuse;
uniform lowp float		fShininess;

varying mediump vec2  TexCoord;
varying highp   vec3  L;
varying highp   vec3  vHalfVector;

void main()
	{
	highp vec3 normal = texture2D(sNormMap, TexCoord).rgb * 2.0 - 1.0;		// Get normal and expand from 0.0 > 1.0 to -1.0 > 1.0
	normal = normalize(normal);		// Looks really shitty without this
	highp vec3 vL = normalize(L);	// Need to normalise due to interpolation
	
	lowp float NdotL = max(dot(normal, vL), 0.0);
	mediump vec3 vDiff = NdotL * vDiffuse;
	mediump vec3 vSpec = vec3(0.0);
	
	if(NdotL > 0.0)
		{
		highp float NdotH = max(dot(normal, vHalfVector), 0.0);		
		vSpec = pow(NdotH, fShininess) * vSpecular;
		}
  
	gl_FragColor = vec4(vDiff + vSpec, 1.0);
	}
