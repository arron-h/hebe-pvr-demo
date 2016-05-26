uniform sampler2D sTexture;

varying mediump		vec2 TexCoord0;
varying mediump		vec2 TexCoord1;
varying mediump		vec2 TexCoord2;

const mediump float fBias = 1.0 / 3.0;

void main()
	{
	lowp vec3 vCol = vec3(0.0);

	vCol  = texture2D(sTexture, TexCoord0).rgb * fBias;
	vCol += texture2D(sTexture, TexCoord1).rgb * fBias;
	vCol += texture2D(sTexture, TexCoord2).rgb * fBias;	
	
	gl_FragColor.rgb = vCol;
	}