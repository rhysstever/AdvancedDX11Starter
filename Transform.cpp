#include "Transform.h"

using namespace DirectX;


Transform::Transform()
{
	// Start with an identity matrix and basic transform data
	XMStoreFloat4x4(&worldMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixIdentity());

	position = XMFLOAT3(0, 0, 0);
	pitchYawRoll = XMFLOAT3(0, 0, 0);
	scale = XMFLOAT3(1, 1, 1);

	// No need to recalc yet
	matricesDirty = false;

	parent = NULL;
}

void Transform::MoveAbsolute(float x, float y, float z)
{
	position.x += x;
	position.y += y;
	position.z += z;
	matricesDirty = true;
}

void Transform::MoveRelative(float x, float y, float z)
{
	// Create a direction vector from the params
	// and a rotation quaternion
	XMVECTOR movement = XMVectorSet(x, y, z, 0);
	XMVECTOR rotQuat = XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));

	// Rotate the movement by the quaternion
	XMVECTOR dir = XMVector3Rotate(movement, rotQuat);

	// Add and store, and invalidate the matrices
	XMStoreFloat3(&position, XMLoadFloat3(&position) + dir);
	matricesDirty = true;
}

void Transform::Rotate(float p, float y, float r)
{
	pitchYawRoll.x += p;
	pitchYawRoll.y += y;
	pitchYawRoll.z += r;
	matricesDirty = true;
}

void Transform::Scale(float x, float y, float z)
{
	scale.x *= x;
	scale.y *= y;
	scale.z *= z;
	matricesDirty = true;
}

void Transform::SetPosition(float x, float y, float z)
{
	position.x = x;
	position.y = y;
	position.z = z;
	matricesDirty = true;
}

void Transform::SetRotation(float p, float y, float r)
{
	pitchYawRoll.x = p;
	pitchYawRoll.y = y;
	pitchYawRoll.z = r;
	matricesDirty = true;
}

void Transform::SetScale(float x, float y, float z)
{
	scale.x = x;
	scale.y = y;
	scale.z = z;
	matricesDirty = true;
}

DirectX::XMFLOAT3 Transform::GetPosition() { return position; }

DirectX::XMFLOAT3 Transform::GetPitchYawRoll() { return pitchYawRoll; }

DirectX::XMFLOAT3 Transform::GetScale() { return scale; }


DirectX::XMFLOAT4X4 Transform::GetWorldMatrix()
{
	UpdateMatrices();

	return worldMatrix;
}

DirectX::XMFLOAT4X4 Transform::GetWorldInverseTransposeMatrix()
{
	UpdateMatrices();
	return worldMatrix;
}

void Transform::AddChild(Transform* child)
{
	// If the new child is null 
	if (child == NULL)
		return;

	// If the new child is already in the list
	for (int i = 0; i < children.size(); i++) {
		if (children[i] == child)
			return;
	}
	
	// Add the new child
	children.push_back(child);

	child->AdjustForParent(true);

	// Set the new child's parent
	child->parent = this;

	MarkChildTransformDirty();
}

void Transform::RemoveChild(Transform* child)
{
	children.erase(children.begin() + IndexOfChild(child));

	child->AdjustForParent(false);
	MarkChildTransformDirty();
}

void Transform::SetParent(Transform* newParent)
{
	// Sets new parent
	parent = newParent;

	// If the new parent is not null, add *this* Transform to its list of children
	if (newParent != NULL) {
		newParent->AddChild(this);
		this->matricesDirty = true;
	}

	MarkChildTransformDirty();
}

Transform* Transform::GetParent()
{
	return parent;
}

Transform* Transform::GetChild(unsigned int index)
{
	// Returns null if the index is out of bounds of the children vector
	if (index < 0 || index >= children.size())
		return NULL;
	
	return children[index];
}

int Transform::IndexOfChild(Transform* child)
{
	// Loops through the child vector and returns the index if one matches
	for (int i = 0; i < children.size(); i++) {
		if (children[i] == child)
			return i;
	}

	// Otherwise returns -1
	return -1;
}

unsigned int Transform::GetChildCount()
{
	return children.size();
}

void Transform::UpdateMatrices()
{
	// Are the matrices out of date (dirty)?
	if (matricesDirty)
	{
		// Create the three transformation pieces
		XMMATRIX trans = XMMatrixTranslationFromVector(XMLoadFloat3(&position));
		XMMATRIX rot = XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));
		XMMATRIX sc = XMMatrixScalingFromVector(XMLoadFloat3(&scale));

		// Combine and store the world
		XMMATRIX wm = sc * rot * trans;

		if (parent != NULL)
		{
			DirectX::XMFLOAT4X4 parentVal = parent->GetWorldMatrix();
			XMMATRIX parentWorldMat = XMLoadFloat4x4(&parentVal);
			wm = XMMatrixMultiply(wm, parentWorldMat);
		}

		XMStoreFloat4x4(&worldMatrix, wm);

		// Invert and transpose, too
		XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixInverse(0, XMMatrixTranspose(wm)));

		// All set
		matricesDirty = false;
		MarkChildTransformDirty();
	}
}

void Transform::MarkChildTransformDirty()
{
	matricesDirty = true;
	for (auto c : children)
	{
		c->MarkChildTransformDirty();
	}
}

void Transform::AdjustForParent(bool isBeingAdded)
{
	// Ensure there is a parent to be adjusted against
	if (parent == NULL)
		return;

	// Scale ============================
	// If the child is being added to the parent, 
	// it need to be scaled DOWN by the scale of the parent, 
	// if the child is being removed from the parent, 
	// its scaled needs to be reverted
	if (isBeingAdded)
		this->Scale(1 / parent->scale.x, 1 / parent->scale.y, 1 / parent->scale.z);
	else
		this->Scale(parent->scale.x, parent->scale.y, parent->scale.z);

	// Position ====================
	// If the child is being added to the parent, 
	// its parent's position needs to be subtracted from the child's 
	// if the child is being removed from the parent, 
	// its parent's position needs to be added
	if (isBeingAdded)
		this->MoveRelative(-parent->position.x, -parent->position.y, -parent->position.z);
	else
		this->MoveRelative(parent->position.x, parent->position.y, parent->position.z);
}

void DescaleFromParent(Transform* parent)
{
	if (parent->GetParent() != NULL)
	{

	}
	else
	{
		 
	}
}