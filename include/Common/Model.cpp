#include "Model.h"

using namespace ci;

Model::Model()
	: mModelMatrix(mat4(1.0f)), mNormalMatrix(mat3(1.0f))
{
}

Model& Model::modelMatrix(const mat4& m)
{
	mModelMatrix = m;
	return *this;
}

Model& Model::normalMatrix(const mat3& m)
{
	mNormalMatrix = m;
	return *this;
}

Model & Model::modelInfo(const ci::vec3 & m)
{
	mModelInfo = m;
	return *this;
}

const ci::vec3 & Model::getModelInfo() const
{
	return mModelInfo;
}

const mat4& Model::getModelMatrix() const
{
	return mModelMatrix;
}

const mat3& Model::getNormalMatrix() const
{
	return mNormalMatrix;
}

void Model::setModelInfo(const ci::vec3 & m)
{
	mModelInfo = m;
}

void Model::setModelMatrix(const mat4& m)
{
	mModelMatrix = m;
}

void Model::setNormalMatrix(const mat3& m)
{
	mNormalMatrix = m;
}
