#include "Protein.h"
#include <boost/algorithm/string.hpp> 
#include "Common/Utils.h"
#include <iostream>
#include <fstream>
#include "cinder/ObjLoader.h"

namespace pdb
{

Protein::Protein()
{

}

Protein::~Protein()
{

}

/*
	Protein structure functions
*/

void Protein::moveTo(glm::vec3 position)
{
	// Prepare translation matrix
	glm::mat4 translation = glm::mat4();
	translation = glm::translate(translation, position);
	// Update bounding Box
	mBoundingBoxMatrix = translation * mBoundingBoxMatrix;

	std::vector<AtomRef>::iterator begin = mAtoms.begin();
	std::vector<AtomRef>::iterator end = mAtoms.end();

	while (begin != end)
	{
		(*begin)->setPosition(mBoundingBoxMatrix);
		++begin;
	}
}

void Protein::setBounds(glm::vec3 position)
{
	// Lower Corner of Bounding Box
	if (position.x < mLowerBound.x) mLowerBound.x = position.x;
	if (position.y < mLowerBound.y) mLowerBound.y = position.y;
	if (position.z < mLowerBound.z) mLowerBound.z = position.z;

	// Upper Corner of Bounding Box
	if (position.x > mUpperBound.x) mUpperBound.x = position.x;
	if (position.y > mUpperBound.y) mUpperBound.y = position.y;
	if (position.z > mUpperBound.z) mUpperBound.z = position.z;
}

void Protein::setBoundingBox()
{
	// Set size of structure
	mSizeOfStructure = ci::distance(glm::vec4(mLowerBound, 1.0f), glm::vec4(mUpperBound, 1.0f));

	// Center of current bounding box
	glm::vec3 centerOfBox;
	centerOfBox = (mLowerBound + mUpperBound) * 0.5f;

	// Set Bounding Box Matrix
	mBoundingBoxMatrix = glm::mat4();
	mBoundingBoxMatrix = glm::translate(mBoundingBoxMatrix, -centerOfBox);
}

/*
	Color scheme functions
*/
void Protein::loadColorScheme(const ci::DataSourceRef dataRef)
{
	// Read the data file
	std::string data;
	try { data = loadString(dataRef); }
	catch (...) { throw ProteinInvalidSourceExc(); }

	// Parse
	try
	{
		std::vector<std::string> lines = ci::split(data, "\n\r");
		for (size_t i = 0; i < lines.size(); ++i)
		{
			std::vector<std::string> tokens = ci::split(lines[i], ";");
			if (!tokens.empty() && tokens.size() == 4)
			{
				// Name of Atom
				std::string tmpName = tokens[0];

				// Color of Atom
				glm::vec3 tmpColor;
				tmpColor.r = boost::lexical_cast<float> (tokens[1]);
				tmpColor.g = boost::lexical_cast<float> (tokens[2]);
				tmpColor.b = boost::lexical_cast<float> (tokens[3]);

				mColorScheme.emplace(tmpName, tmpColor);
			}
		}
	}
	catch (...) { throw ProteinInvalidSourceExc(); }
}

/*
	Atom Properties functions
*/
void Protein::loadAtomRadii(const ci::DataSourceRef dataRef)
{
	// Read the data file
	std::string data;
	try { data = loadString(dataRef); }
	catch (...) { throw ProteinInvalidSourceExc(); }

	// Parse
	try
	{
		std::vector<std::string> lines = ci::split(data, "\n\r");
		for (size_t i = 0; i < lines.size(); ++i)
		{
			std::vector<std::string> tokens = ci::split(lines[i], ";");
			if (!tokens.empty() && tokens.size() == 2)
			{
				// Name of Atom
				std::string tmpName = tokens[0];

				// Radii of Atom
				float tmpRadii;
				tmpRadii = boost::lexical_cast<float> (tokens[1]);
				tmpRadii /= 100.0f;

				mAtomRadii.emplace(tmpName, tmpRadii);
			}
		}
	}
	catch (...) { throw ProteinInvalidSourceExc(); }
}

void Protein::loadPdb(const ci::DataSourceRef dataRef)
{
	// Read the data file
	std::string data;
	try { data = loadString(dataRef); }
	catch (...) { throw ProteinInvalidSourceExc(); }

	// Parse the file
	try 
	{
		// Split the file into the lines
		std::vector<std::string> lines = ci::split(data, "\n\r");
		if (lines.size() < 2) throw;

		// Prepare mAtoms
		if(!mAtoms.empty()) mAtoms.clear();
		mAtoms.reserve(lines.size());

		// Read all lines
		for (size_t i = 0; i < lines.size(); ++i)
		{
			if (lines[i].substr(0, 4) == "ATOM")
			{
				// ---------------------------------------------
				// Create Atom
				// ---------------------------------------------
				
				// Atom serial number
				int	tmpId = boost::lexical_cast<int> (boost::trim_copy(lines[i].substr(6, 5)));

				// Position of Atom
				glm::vec3 tmpPosition;
				tmpPosition.x = boost::lexical_cast<float> (boost::trim_copy(lines[i].substr(30, 7)));
				tmpPosition.y = boost::lexical_cast<float> (boost::trim_copy(lines[i].substr(38, 7)));
				tmpPosition.z = boost::lexical_cast<float> (boost::trim_copy(lines[i].substr(46, 7)));

				// Name of Atom
				std::string tmpName = lines[i].substr(77, 1);

				mAtoms.emplace_back(new Atom(tmpId, tmpName, tmpPosition));

				// ---------------------------------------------
				// Set Atom Properties
				// ---------------------------------------------
				
				auto searchColor = mColorScheme.find(tmpName);
				auto searchRadii = mAtomRadii.find(tmpName);

				// Set Color
				if (searchColor != mColorScheme.end())
					mAtoms.back()->setColor(searchColor->second);
				else
					mAtoms.back()->setColor(glm::vec3(0.0f));
				
				// Set Radii
				if (searchRadii != mAtomRadii.end())
					mAtoms.back()->setRadii(searchRadii->second);
				else
					mAtoms.back()->setRadii(2.0f);

				// ---------------------------------------------
				// Extras
				// ---------------------------------------------

				// set new minValues, maxValues for Boudning Box
				setBounds(tmpPosition);
			}
		}
		// compute Bouding Box Matrix 
		setBoundingBox();
	}
	catch (...) { throw ProteinInvalidSourceExc(); }
}

/*
   Public function
*/

void Protein::loadProtein(const ci::DataSourceRef colorDataRef, const ci::DataSourceRef radiiDataRef, const ci::DataSourceRef pdbDataRef)
{
	try 
	{
		cleanUp();

		mName = utils::remove_extension(pdbDataRef->getFilePath().string());

		const size_t last_slash_idx = mName.find_last_of("\\/");
		if (std::string::npos != last_slash_idx)
		{
			mName.erase(0, last_slash_idx + 1);
		}
		
		// Load protein data
		loadColorScheme(colorDataRef);
		loadAtomRadii(radiiDataRef);
		loadPdb(pdbDataRef);

		// Center protein structure
		moveTo(glm::vec3(0.0f));
	}
	catch (...) { throw ProteinInvalidSourceExc(); }
}

void Protein::cleanUp()
{
	float minValue = std::numeric_limits<float>::min();
	float maxValue = std::numeric_limits<float>::max();

	mLowerBound = glm::vec3(maxValue);
	mUpperBound = glm::vec3(minValue);
	mBoundingBoxMatrix = glm::mat4();
	mSizeOfStructure = 0;

	if (!mAtoms.empty())			mAtoms.clear();
	if (!mColorScheme.empty())		mColorScheme.clear();
	if (!mAtomRadii.empty())		mAtomRadii.clear();
	if (!mSelected.empty())			mSelected.clear();
}

bool Protein::select(int atomId)
{
	if (atomId < 0 || (size_t)atomId > mAtoms.size()) return false;

	auto search = mSelected.find(atomId);
	if (search != mSelected.end())
		mSelected.erase(atomId);
	else
		mSelected.insert(atomId);
		
	return true;
}

}	// namespace Protein
