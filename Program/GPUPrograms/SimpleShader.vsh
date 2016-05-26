attribute highp		vec3	inPosition;

uniform highp	mat4	mxMVP;

void main()
	{
	gl_Position = mxMVP * vec4(inPosition, 1.0);
	}