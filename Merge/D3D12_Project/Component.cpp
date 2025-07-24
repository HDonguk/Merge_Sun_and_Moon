#include "Component.h"
#include "Object.h"
#include "Scene.h"

Transform::Transform(XMVECTOR pos, XMVECTOR rot, XMVECTOR scale)
{
	XMStoreFloat3(&mScale, scale);
	XMStoreFloat3(&mRotation, rot);
	XMStoreFloat3(&mPosition, pos);
	//XMStoreFloat4(&mQuaternion, XMQuaternionNormalize(GetQuaternionFromRotation()));
	XMStoreFloat4x4(&mFinalM, GetTransformM());
}

XMVECTOR Transform::GetScale()
{
	return XMVectorSet(mScale.x, mScale.y, mScale.z, 0.0f);
}

XMVECTOR Transform::GetRotation()
{
	return XMVectorSet(mRotation.x, mRotation.y, mRotation.z, 0.0f);
}

XMVECTOR Transform::GetQuaternion()
{
	return  XMQuaternionNormalize(XMVectorSet(mQuaternion.x, mQuaternion.y, mQuaternion.z, mQuaternion.w));
}

XMVECTOR Transform::GetPosition()
{
	return XMVectorSet(mPosition.x, mPosition.y, mPosition.z, 1.0f);
}

XMMATRIX Transform::GetTranslateM()
{
	return XMMatrixTranslationFromVector(GetPosition());
}

XMMATRIX Transform::GetScaleM()
{
	return XMMatrixScalingFromVector(GetScale());
}

XMMATRIX Transform::GetRotationM()
{
	XMVECTOR rot = GetRotation();
	return XMMatrixRotationRollPitchYawFromVector(rot * XM_PI / 180);
}

XMMATRIX Transform::GetRotationQuaternionM()
{
	return XMMatrixRotationQuaternion(GetQuaternion());
}

XMMATRIX Transform::GetTransformM()
{
	return GetScaleM() * GetRotationM() * GetTranslateM();
}

XMMATRIX Transform::GetFinalM()
{
	return XMLoadFloat4x4(&mFinalM);
}

	XMVECTOR Transform::GetQuaternionFromRotation()
	{
		XMVECTOR q1 = XMQuaternionRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMConvertToRadians(mRotation.x));
		XMVECTOR q2 = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(mRotation.y));
		XMVECTOR q3 = XMQuaternionRotationAxis(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMConvertToRadians(mRotation.z));
		return XMQuaternionMultiply(XMQuaternionMultiply(q3, q1), q2);
		//return XMQuaternionRotationRollPitchYaw(
		//	XMConvertToRadians(mRotation.x),
		//	XMConvertToRadians(mRotation.y),
		//	XMConvertToRadians(mRotation.z));
	}
	
	void Transform::SetPosition(XMVECTOR pos)
	{
		XMStoreFloat3(&mPosition, pos);
	}
	
	void Transform::SetRotation(XMVECTOR rot)
	{
		XMStoreFloat3(&mRotation, rot);
		XMStoreFloat4(&mQuaternion, GetQuaternionFromRotation());
	}
	
	void Transform::SetQuaternion(XMVECTOR qua)
	{
		XMStoreFloat4(&mQuaternion, qua);
	}
	
	void Transform::SetFinalM(XMMATRIX finalM)
	{
		XMStoreFloat4x4(&mFinalM, finalM);
	}

XMMATRIX AdjustTransform::GetTranslateM()
{
	return XMMatrixTranslationFromVector(XMLoadFloat3(&mPosition));
}

AdjustTransform::AdjustTransform(XMVECTOR pos, XMVECTOR rot, XMVECTOR scale)
{
	XMStoreFloat3(&mScale, scale);
	XMStoreFloat3(&mRotation, rot);
	XMStoreFloat3(&mPosition, pos);
}

XMMATRIX AdjustTransform::GetScaleM()
{
	return XMMatrixScalingFromVector(XMLoadFloat3(&mScale));
}

XMMATRIX AdjustTransform::GetRotationM()
{
	XMVECTOR rot = XMLoadFloat3(&mRotation);
	return XMMatrixRotationRollPitchYawFromVector(rot * XM_PI / 180);
}

XMMATRIX AdjustTransform::GetTransformM()
{
	return GetScaleM() * GetRotationM() * GetTranslateM();
}

XMVECTOR Gravity::ProcessGravity(XMVECTOR pos, float deltaTime)
{
	mElapseTime += deltaTime;
	float gForce = mG * mElapseTime * mElapseTime;
	mVerticalSpeed -= gForce * deltaTime;

	XMFLOAT3 newPos{};
	XMStoreFloat3(&newPos, pos);
	newPos.y += mVerticalSpeed * deltaTime;
	return XMLoadFloat3(&newPos);
}

void Gravity::ResetElapseTime()
{
	// 수직 속도가 양수이면 pos의 y 요소가 증가함을 의미한다.
	// 이때 바닥면과 충돌하더라도 반응하지 않는다. 
	// 수직 속도가 음수일 경우. 즉, pos 의 y요소가 감소하는 경우에만 바닥면 충돌에 반응한다. 

	if (mVerticalSpeed <= 0.0f) {
		mElapseTime = 0.0f;
		mVerticalSpeed = 0.0f;
	}

}

float Gravity::GetElapseTime()
{
	return mElapseTime;
}

void Gravity::SetVerticalSpeed(float speed)
{
	mVerticalSpeed = speed;
}

Collider::Collider(XMFLOAT3&& center, XMFLOAT3&& extents, XMFLOAT4&& orientation) :
	mBaseOBB{ std::move(center), std::move(extents), std::move(orientation) }
{
}

void Collider::UpdateOBB(XMMATRIX M)
{
	mBaseOBB.Transform(mOBB, M);
	XMStoreFloat4(&mOBB.Orientation, XMQuaternionNormalize(XMLoadFloat4(&mOBB.Orientation)));
}

BoundingOrientedBox& Collider::GetOBB()
{
	return mOBB;
}

Animation::Animation(string initFileName) : mCurrentFileName{initFileName}
{
}

void Animation::ResetAnim(string fileName, float time)
{
	if (fileName == mCurrentFileName) return;
	mCurrentFileName = fileName;
	mAnimationTime = time;
}
