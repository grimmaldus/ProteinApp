#pragma once

#include "cinder/Matrix.h"

class Model
{
public:
	Model();

	Model&			modelMatrix(const ci::mat4& m);
	Model&			normalMatrix(const ci::mat3& m);
	Model&			modelInfo(const ci::vec3& m);

	const ci::vec3&	getModelInfo() const;
	const ci::mat4&	getModelMatrix() const;
	const ci::mat3&	getNormalMatrix() const;

	void			setModelInfo(const ci::vec3& m);
	void			setModelMatrix(const ci::mat4& m);
	void			setNormalMatrix(const ci::mat3& m);
protected:
	ci::vec3		mModelInfo;
	ci::mat4		mModelMatrix;
	ci::mat3		mNormalMatrix;
};
