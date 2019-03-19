#pragma once

#include "cinder/Utilities.h"
#include "cinder/CinderGlm.h"

typedef std::shared_ptr<class Atom> AtomRef;

class Atom
{
public:
	Atom(int id ,std::string name, glm::vec3 position);
	~Atom();
protected:
	int			mId;
	std::string mName;
	glm::vec3	mPosition;

	// Atom physical properties
	float		mRadii; // Van der Waals radius
protected: // Extras
	
	// Atom world matrix
	glm::mat4   mMatrix;

	// Atom color
	glm::vec3	mColor;
public: // Mutators
	int			const &getId()					{ return mId; }
	std::string const &getName()				{ return mName; }
	glm::vec3	const &getPosition()			{ return mPosition; }
	float		const &getRadii()				{ return mRadii; }
	
	void setName(std::string name)				{ mName = name; }
	void setPosition(glm::vec3 position)		{ mPosition = position; }
	void setPosition(glm::mat4 transformation);
	void setRadii(float radii)					{ mRadii = radii; }

	// Extras
	glm::vec3   const &getColor()				{ return mColor; }
	glm::mat4   const &getMatrix()				{ return mMatrix; }

	void setColor(glm::vec3 color)				{ mColor = color; }
	void setMatrix(glm::mat4 matrix)			{ mMatrix = matrix; }
};
