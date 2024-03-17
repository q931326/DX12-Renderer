#include "MathHelper.h"
#include <float.h>
#include <cmath>

using namespace DirectX;

const float MathHelper::Infinity = FLT_MAX;
const float MathHelper::Pi       = 3.1415926535f;

float MathHelper::AngleFromXY(float x, float y)
{
	float theta = 0.0f;
 
	// 第一象限或第四象限
	if(x >= 0.0f) 
	{
		// If x = 0, then atanf(y/x) = +pi/2 if y > 0
		//                atanf(y/x) = -pi/2 if y < 0
		theta = atanf(y / x); // in [-pi/2, +pi/2]

		if(theta < 0.0f)
			theta += 2.0f*Pi; // in [0, 2*pi).
	}

	// 第二象限或第三象限
	else      
		theta = atanf(y/x) + Pi; // in [0, 2*pi).

	return theta;
}

XMVECTOR MathHelper::RandUnitVec3()
{
	XMVECTOR One  = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	// 不断尝试，直到在半球/半球得到一个点
	while(true)
	{
		// 在立方体 [-1,1]^3 中生成随机点。
		XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

		// 忽略单位球体外的点，以便在单位球体上获得均匀分布。否则，点会更多地聚集在靠近
		// 立方体角的球体上

		if( XMVector3Greater( XMVector3LengthSq(v), One) )
			continue;

		return XMVector3Normalize(v);
	}
}

XMVECTOR MathHelper::RandHemisphereUnitVec3(XMVECTOR n)
{
	XMVECTOR One  = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	while(true)
	{
		XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

		//忽略单位球体外的点，以便在单位球体上获得均匀分布。 否则，点会更多地聚集在靠近立方体角
		// 的球体上。
		
		if( XMVector3Greater( XMVector3LengthSq(v), One) )
			continue;

		// 忽略下半球的点
		if( XMVector3Less( XMVector3Dot(n, v), Zero ) )
			continue;

		return XMVector3Normalize(v);
	}
}