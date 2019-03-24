#pragma once

#include "cinder/Cinder.h"
#include "cinder/Utilities.h"
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include <set>

#include "Atom.h"

namespace pdb 
{

typedef std::shared_ptr<class Protein> ProteinRef;

class Protein
{
public:
	Protein();
	~Protein();
protected:
	// Name
	std::string				mName;

	// Color Scheme
	std::map<std::string, glm::vec3>	mColorScheme;

	// Atom properties
	std::map<std::string, float>		mAtomRadii;

	// Atom Containers
	std::vector<AtomRef>			mAtoms;	    // Order given by pdb; ID of atom is its position in container; Not safe but enough for our Prototype   

	// Protein structure properties
	glm::vec3				mUpperBound;
	glm::vec3				mLowerBound;
	glm::mat4				mBoundingBoxMatrix;

	// Selection
	std::set<int>				mSelected;  // Container of selected atoms, for efficient handling (Assuming that selection will be smaller than whole atoms)  

	float					mSizeOfStructure;

	// Secondary structures


protected:
	// Color Scheme functions
	void loadColorScheme(const ci::DataSourceRef dataRef);

	// Atom Properties functions
	void loadAtomRadii(const ci::DataSourceRef dataRef);
	void loadPdb(const ci::DataSourceRef dataRef);
	//void loadSecStructures(const ci::DataSourceRef dataRef);

	// Protein structure functions
	void setBounds(glm::vec3 position);
	void setBoundingBox();
	void moveTo(glm::vec3 position); 
public: // Functions
	void loadProtein(const ci::DataSourceRef colorDataRef,
			 const ci::DataSourceRef radiiDataRef,
			 const ci::DataSourceRef pdbDataRef);
	// Clean up
	void cleanUp();

	// Select
	bool select(int atomId);	// (de)select
public:	// Mutators
	
	std::vector<AtomRef>			const &getAtoms()		{ return mAtoms; }
	std::set<int>				const &getSelected()		{ return mSelected; }
	glm::mat4				const &getBoundingBoxMatrix()   { return mBoundingBoxMatrix; }
	glm::vec3				const &getBoundUpper()		{ return mLowerBound; }
	glm::vec3				const &getBoundLower()		{ return mUpperBound; }
	float					const &getSizeOfStructure()	{ return mSizeOfStructure; }
};

class ProteinExc : public std::exception {
public:
	virtual const char* what() const throw() { return "Protein exception"; }
};

class ProteinInvalidSourceExc : public ProteinExc {
public:
	virtual const char* what() const throw() { return "Protein exception: could not load from the specified source"; }
};

} // namespace pdb

/*
	TBD:
	1) Abstract class for Atoms
		DClasses:
		a) HAtom
		b) Selected

		DDClasses:
		a) RenderAtom (Special cases)

	2) Abstract class for Protein
		DMethods:
		a) Render (Only for special cases - not needed for instance rendering)

		DClasses:
		a) Substructures

		DDClasses:
			a) RenderAtom
			
			DMethods:
			a) Render (Batch generator at least)
	3)	Selection Class
		-Handling with selected atoms
		-Efficient handling with bounds of selected area (Unlike in this class, store upper and lower bounds in sorted lists)
*/
