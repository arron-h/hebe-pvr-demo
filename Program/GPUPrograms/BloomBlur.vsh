attribute highp		vec2 inVertex;
attribute mediump	vec2 inTexCoord;

uniform mediump		vec2 vTexelOffset;

varying mediump		vec2 TexCoord0;
varying mediump		vec2 TexCoord1;
varying mediump		vec2 TexCoord2;

void main()
	{
	gl_Position = vec4(inVertex, 0.0, 1.0);
	
	// 3x3 Sample (actually 5x5 but using bilinear interpolation to filter the two surrounding texels for us).
	TexCoord0 = inTexCoord - vTexelOffset;
	TexCoord1 = inTexCoord;
	TexCoord2 = inTexCoord + vTexelOffset;
	}