#include "Atom.h"

Atom::Atom(int id, std::string name, glm::vec3 position)
{
	this->mId	= id;
	this->mName	= name;
	this->mPosition = position;
	this->mMatrix	= glm::mat4();
}

Atom::~Atom()
{
}

void Atom::setPosition(glm::mat4 transformation)
{
	mPosition = glm::vec3(transformation * glm::vec4(mPosition, 1.0f));
}
