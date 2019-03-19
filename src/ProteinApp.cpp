#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Utilities.h"
#include "cinder/ObjLoader.h"
#include "cinder/Capture.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "boost/algorithm/string.hpp"
#include "cinder/params/Params.h"
#include <boost/algorithm/string.hpp> 

#include "ProteinLoader/PDBloader.h"
#include "ProteinLoader/Utils.h"

#define DEBUG

using namespace ci;
using namespace ci::app;
using namespace std;

struct ShaderData
{
	float	strength;
	bool	enable;

	// Absorbtion / Thickness calculation technique
	int		thickTechnique;
	bool	thicknessEnable;

	// Lighting model
	int		lightingModel;
	bool	transmitanceEnable;
	float	extintionCoef;
	bool    irradianceEnable;
	float   irradianceFix;
};

struct LightData
{
	vec3		position;
	vec3		color;
	CameraOrtho cam;

	// Attenuation
	float	    constant;
	float		linear;
	float		quadratic;

	// Animation
	bool		animated;
};



class ProteinApp : public App {
public:
	static void prepare(Settings *settings);

	void setup() override;
	void update() override;
	void draw() override;

	void resize();

	void mouseMove(MouseEvent event) override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseUp(MouseEvent event) override;

	void keyDown(KeyEvent event) override;
	void keyUp(KeyEvent event) override;

	void fileDrop(FileDropEvent event) override;
private:
	// GUI
	void initializeGUI();

	// Perform Picking
	bool performPicking(float mouseX , float mouseY);

	// Load an object mesh into a VBO using ObjLoader
	void loadMesh();

	// Creates a VAO containing a transform matrix for each instance
	void initializeBuffer();

	// Depth Map
	void renderToFBO();
private:
	gl::FboRef					mFboTest;
	gl::FboRef					mFboTestPicking;
	gl::BatchRef				mBatchTest;
	gl::GlslProgRef				mShaderTest;
	void renderToTestFbo();
	bool pickByColor(const ivec2 &position);
private:
	// GUI
	params::InterfaceGlRef		mParams;
	float						mAvgFrameRate;
	float						mBackGroundColor;

	// Pick
	TriMeshRef					mTriMesh;
	AxisAlignedBox				mObjectBounds;
	std::map<int,bool>			mPicked;

	// Controlable camera
	CameraPersp					mCamera;
	CameraUi					mCameraUi;

	// Shader instanced rendering support
	gl::GlslProgRef				mShader;
	// VBO containing one object(sphere) mesh
	gl::VboMeshRef				mVboMesh;
	// Batch combining mesh and shader
	gl::BatchRef				mBatch;

	// Wire meshes
	gl::BatchRef				mWirePlane;
	gl::BatchRef				mWireBoundingCube;
	std::vector< mat4 >			mModelMatrices;

	// PDB file
	pdb::PDBloaderRef			mPDB;
	float						mSizeOfStructure;
	float						mSizeOfAtoms;

	// VBO containing a list of matrices, one for every instance
	gl::VboRef					mInstanceDataVbo;
	// VBO containing a list of colors, one for every instance
	gl::VboRef					mInstanceColorVbo;

	// Depth Map
	LightData					mLight;
	gl::FboRef					mFboDepthMap;

	// Subsurface Scattering Shader Data
	ShaderData					mShaderData;
};

void ProteinApp::prepare(Settings * settings)
{
	settings->setWindowSize(800, 600);
	settings->setTitle("Protein Visualizer");
	settings->setFrameRate(500.0f);
}

void ProteinApp::setup()
{
	// Load shaders
	try
	{
		mShader = gl::GlslProg::create(loadAsset("program.vert"), loadAsset("program.frag"));
		mShaderTest = gl::GlslProg::create(loadAsset("picker.vert"), loadAsset("picker.frag"));
	}
	catch (const std::exception &e)
	{
		console() << "Could not load and compile shader: " << e.what() << std::endl;
	}

	// Stock shader 
	auto colorShader = gl::getStockShader(gl::ShaderDef().color());

	// Measure 
	mAvgFrameRate = 0.0f;

	// App globals
	mBackGroundColor = 0.0f;

	// Structure options
	mSizeOfAtoms = 2.0f;

	// Shader Data
	mShaderData.strength = 1.0f;
	mShaderData.enable = true;
	mShaderData.thickTechnique = 0;
	mShaderData.lightingModel = 0;
	mShaderData.thicknessEnable = false;
	mShaderData.transmitanceEnable = false;
	mShaderData.extintionCoef = 30.0f;
	mShaderData.irradianceEnable = false;
	mShaderData.irradianceFix = 0.3f;

	// Light Data
	mLight.position = vec3(0.0, 0.0f, 50.0f);
	mLight.color = vec3(1.0f, 1.0f, 1.0f);
	mLight.cam.setOrtho(-125.0f, 125.0f, -125.0f, 125.0f, 1.0f, 500.0f);
	mLight.cam.lookAt(mLight.position, vec3(0.0f), vec3(0.0f, -1.0f, 0.0f));
	mLight.animated = true;
	
	// Light Data - Attenuation
	mLight.constant = 1.0f;
	mLight.linear = 0.0014f;
	mLight.quadratic = 0.000007;

	// Setup Camera
	mCamera.setPerspective(40.0f, getWindowAspectRatio(), 1.0f, 5000.0f);
	mCamera.lookAt(vec3(90.0f, 90.0f, 90.0f), vec3(0.0f));
	mCameraUi.setCamera(&mCamera);
	mShader->uniform("uNearFarPlane", vec2(mCamera.getNearClip(), mCamera.getFarClip()));

	// Depth Map
	{
		const GLsizei size_of_map = 2048;
		gl::Fbo::Format format;
		mFboDepthMap = gl::Fbo::create(size_of_map, size_of_map, format.depthTexture());
		const gl::ScopedFramebuffer scopedFramebuffer(mFboDepthMap);
		const gl::ScopedViewport scopedViewport(ivec2(0), mFboDepthMap->getSize());
		gl::clear();
		//mFboDepthMap->getDepthTexture()->setCompareMode(GL_COMPARE_REF_TO_TEXTURE);
	}

	// Wire meshes
	mWirePlane = gl::Batch::create(geom::WirePlane().size(vec2(10)).subdivisions(ivec2(10)), colorShader);
	mWireBoundingCube = gl::Batch::create(geom::WireCube(), colorShader);

	// Load object mesh
	loadMesh();
	
	// SetGUI
	initializeGUI();

	// PDB
	mSizeOfStructure = 250.0f;

	// Load radii
	mPDB = pdb::PDBloaderRef (new pdb::PDBloader());

	// Set render States
	//gl::enableFaceCulling();
	gl::enableDepthRead();
	gl::enableDepthWrite();
}


void ProteinApp::update()
{
	// Measure
	mAvgFrameRate = getAverageFps();
	// Camera Data
	mShader->uniform("uEyePos", vec3(mCamera.getEyePoint()));

	// Update position of Light
	if(mLight.animated)
	{
		mLight.position.z = (float) sin(getElapsedSeconds())* (mSizeOfStructure + 20.0f);
		mLight.position.x = (float) cos(getElapsedSeconds())* (mSizeOfStructure + 20.0f);
		mLight.cam.lookAt(mLight.position, vec3(0.0f), vec3(0.0f, -1.0f, 0.0f));
	}	

	// Update Light Data Uniforms
	mShader->uniform("uLightPos", mLight.position);
	mShader->uniform("uLightColor", mLight.color);

	// Update Light Attenuation
	mShader->uniform("uConstant", mLight.constant);
	mShader->uniform("uLinear", mLight.linear);
	mShader->uniform("uQuadratic", mLight.quadratic);

	// Update Shader Data Uniforms
	mShader->uniform("uStrength", mShaderData.strength);
	mShader->uniform("uEnable", mShaderData.enable);
	mShader->uniform("uThickTechnique", mShaderData.thickTechnique);
	mShader->uniform("uLightingModel", mShaderData.lightingModel);
	mShader->uniform("uThicknessEnable", mShaderData.thicknessEnable);
	mShader->uniform("uTransmitanceEnable", mShaderData.transmitanceEnable);
	mShader->uniform("uExtintionCoef", mShaderData.extintionCoef);
	mShader->uniform("uIrradianceEnable", mShaderData.irradianceEnable);
	mShader->uniform("uIrradianceFix", mShaderData.irradianceFix);
}

void ProteinApp::draw()
{
	// Clear the window
	gl::clear(vec4(vec3(0.1f), 1.0f));

	// Render to FBO
	renderToFBO();
	renderToTestFbo();
	// Activate our camera
	gl::setMatrices(mCamera);

#ifdef DEBUG
	// Draw the grid & Light.
	{
		if (!mPicked.empty())
		for (auto const &ent1 : mPicked)
			if (ent1.second)
			{
				gl::pushModelMatrix();
				gl::multModelMatrix(mModelMatrices[ent1.first]);
				mWireBoundingCube->draw();
				gl::popModelMatrix();
			}
	}
	{
		gl::ScopedColor color(Color::gray(0.2f));
		mWirePlane->draw();
	}
#endif
	{
		gl::ScopedColor color(Color::gray(0.8f));
		gl::drawVector(mLight.position, 4.5f * normalize(mLight.position));
	}

	// Draw Instances
	if (mVboMesh && mShader && mInstanceDataVbo)
	{
		gl::pushMatrices();
		// Number of Instances = number of atoms in pdb
		unsigned int numOfAtoms = mPDB->getAtoms().size();

		gl::ScopedGlslProg shader(mShader);
		mShader->uniform("uDepthMap", 0); //Depth map from FBO
		gl::ScopedTextureBind uDepthMap(mFboDepthMap->getDepthTexture(), (uint8_t)0);
		mShader->uniform("uLightViewProjMatrix", mLight.cam.getProjectionMatrix() * mLight.cam.getViewMatrix());
		mBatch->drawInstanced(numOfAtoms);
		gl::popMatrices();
	}
#ifdef DEBUG
	// restore 2D drawing
	//gl::setMatricesWindow(toPixels(getWindowSize()));
	//gl::draw(mFboDepthMap->getDepthTexture() , Rectf(getWindowWidth() - 256, 256, getWindowWidth(), 0));
#endif
	
	// show the FBO color texture in the upper left corner
	gl::setMatricesWindow(toPixels(getWindowSize()));
	gl::draw(mFboTest->getColorTexture(), Rectf(0, 0, (float)getWindowWidth()/5, (float)getWindowHeight()/5));

	// draw picking buffer
	if (mFboTestPicking) {
		Rectf rct = (Rectf)mFboTestPicking->getBounds() * 5.0f;
		rct.offset(vec2((float)getWindowWidth() - rct.getWidth(), 0));
		gl::draw(mFboTestPicking->getColorTexture(), rct);
	}

	// GUi
	mParams->draw();
}

void ProteinApp::resize()
{
	// Adjust the camera aspect ratio	
	mCamera.setAspectRatio(getWindowAspectRatio());

	// Test FBO
	gl::Fbo::Format format;
	format.setSamples( 0 ); // uncomment this to enable 4x antialiasing
	mFboTest = gl::Fbo::create(getWindowWidth(), getWindowHeight(), format.colorTexture());

}

void ProteinApp::fileDrop( FileDropEvent event )
{
	
	if ( event.getNumFiles() == 1 )
	{
		fs::path file = event.getFile(0);

		if( file.extension() == ".pdb" )
		{
			try
			{
				mPDB->loadProtein(loadFile(file));
				initializeBuffer();
			}
			catch (const std::exception &e)
			{
				console() << e.what() << std::endl;
			}
		}
	}
}

void ProteinApp::initializeGUI()
{
	mParams = params::InterfaceGl::create("Settings", toPixels(ivec2(200, 300)));
	mParams->addText("App params");
	mParams->addParam("FPS", &mAvgFrameRate, "", true);
	mParams->addParam("Backgroun Color", &mBackGroundColor).min(0.0f).max(1.0f).step(0.1);

	mParams->addSeparator();
	mParams->addText("Light control");
	mParams->addButton("Stop/Start", [&]() {mLight.animated = !mLight.animated; }, "key=l");

	// Depth map properties
	mParams->addSeparator();
	mParams->addText("Thickness/Absorbtion"); 
	mParams->addParam("Show thickness only",&mShaderData.thicknessEnable, "key=t");
	mParams->addParam("Strength", &mShaderData.strength).min(1.0f).max(10.0f).step(0.2f);
	std::vector<std::string> techniques = { "dOut - dIn", "dOut", "dIn" , "D'eon d"};
	mParams->addParam("Technique", techniques, &mShaderData.thickTechnique);
	
	// Lighting model properties
	mParams->addText("Lighting Model");
	std::vector<std::string> lightingModel = { "Jorge Jimenez Translucency", "Ben's translucency model", "Blin-Phong" };
	mParams->addParam("Current Model", lightingModel, &mShaderData.lightingModel);
	mParams->addParam("Show only Transmitance", &mShaderData.transmitanceEnable, "key=c");
	mParams->addParam("Extinction coefficient", &mShaderData.extintionCoef).min(1.0f).max(1000.0f).step(1.0f);
	mParams->addParam("Show Irradiance", &mShaderData.irradianceEnable, "key=r");
	mParams->addParam("Irradiance Fix", &mShaderData.irradianceFix).min(0.0f).max(1.0f).step(0.1f);

	// Light properties
	mParams->addSeparator();
	mParams->addText("Light Attenuation");
	mParams->addParam("Constant", &mLight.constant, true);
	mParams->addParam("Linear", &mLight.linear).min(0.0014).max(0.7).step(0.0001);
	mParams->addParam("Quadratic", &mLight.quadratic).min(0.000007).max(1.8).step(0.000001);

	// Structure properties
	mParams->addSeparator();
	mParams->addText("Structure options");
	mParams->addParam("Diameter of Atoms", &mSizeOfAtoms).min(2.0f).max(10.0f).step(0.5f);
}

bool ProteinApp::performPicking(float mouseX, float mouseY)
{
	if(!mVboMesh || !mShader || !mInstanceDataVbo) return false;
	
	float u = mouseX / (float)getWindowWidth();
	float v = mouseY / (float)getWindowHeight();
	Ray ray = mCamera.generateRay(u, 1.0f - v, mCamera.getAspectRatio());

	AxisAlignedBox worldBoundsApprox; // fast
	int modelIndex; // modelIndex
	int chosen = -1;
	for (modelIndex = 0; modelIndex < mModelMatrices.size(); ++modelIndex)
	{
		//worldBoundsApprox = mTriMesh->calcBoundingBox(mModelMatrices[modelIndex]); // fast
		
		// Calc intersection localy
		size_t polycount = mTriMesh->getNumTriangles();
		float new_distance = 0.0f;
		float current_distance = FLT_MAX;
		for (size_t i = 0; i < polycount; ++i)
		{
			vec3 v0, v1, v2;
			mTriMesh->getTriangleVertices(i, &v0, &v1, &v2);
			v0 = vec3(mModelMatrices[modelIndex] * vec4(v0, 1.0));
			v1 = vec3(mModelMatrices[modelIndex] * vec4(v1, 1.0));
			v2 = vec3(mModelMatrices[modelIndex] * vec4(v2, 1.0));

			if (ray.calcTriangleIntersection(v0, v1, v2, &new_distance))
			{
				if (new_distance < current_distance)
				{
					current_distance = new_distance;
					chosen = modelIndex;
				}
			}
		}
		
	}
	if (chosen == -1 ) return false;

	if (mPicked.find(chosen) == mPicked.end())
		mPicked.insert(make_pair(chosen, true));
	else
		mPicked.find(chosen)->second = !mPicked.find(chosen)->second;
#ifdef DEBUG
	for (auto const &ent1 : mPicked)
		if (ent1.second) console() << ent1.first << endl;
#endif
	return true;
}

void ProteinApp::loadMesh()
{
	ObjLoader loader(loadAsset("sphere_low_poly.obj"));

	try
	{
		TriMeshRef mesh = TriMesh::create(loader);
		mObjectBounds = mesh->calcBoundingBox();
		mVboMesh = gl::VboMesh::create(*mesh);
		mPicked.clear();
		mTriMesh = mesh;
	}
	catch (const std::exception &e)
	{
		console() << e.what() << std::endl;
	}
}

void ProteinApp::initializeBuffer()
{
	// Clear
	mPicked.clear();

	// Move atoms to middle
	mPDB->moveTo(vec3(0.0f));

	// Set Camera
	mSizeOfStructure = distance(vec4(mPDB->getBoundLower(), 1.0f), vec4(mPDB->getBoundUpper(),1.0f));
	console() << mSizeOfStructure << std::endl;
	mCamera.lookAt(glm::vec3(mSizeOfStructure), glm::vec3(0.0f));

	// Set Light Came
	float orhtoSize = mSizeOfStructure / 2 + 7.5f;
	mLight.cam.setOrtho(-orhtoSize, orhtoSize,
						-orhtoSize, orhtoSize,
						1.0f, mSizeOfStructure*1.5f + 20.0f);

	// Number of Instances = number of atoms in pdb
	unsigned int numOfAtoms = mPDB->getAtoms().size();

	// ---------------------------------------------
	// Model Matrices
	// ---------------------------------------------

	// Init trnasforms for every instance
	mModelMatrices.clear();
	mModelMatrices.reserve(numOfAtoms);

	for (size_t i = 0; i < numOfAtoms; i++)
	{
		mat4 model = translate(mPDB->getAtoms()[i]->getPosition());
		model = scale(model, vec3(mPDB->getAtoms()[i]->getVdWRadii()*2.0f));
		mModelMatrices.push_back(model);
	}

	// Create an array Buffer to store all model matrices
	mInstanceDataVbo = gl::Vbo::create(GL_ARRAY_BUFFER, mModelMatrices.size() * sizeof(mat4), mModelMatrices.data(), GL_STATIC_DRAW);

	// Setup the buffer to contain space for all matrices. Each matrix needs 16 floats
	geom::BufferLayout instanceDataLayout;
	instanceDataLayout.append(geom::Attrib::CUSTOM_0, 16, sizeof(mat4), 0, 1 /* per instance */);

	// Add buffer to mesh 
	mVboMesh->appendVbo(instanceDataLayout, mInstanceDataVbo);

	// ---------------------------------------------
	// Materials
	// ---------------------------------------------

	std::vector< vec3 > matColors;
	matColors.reserve(numOfAtoms);

	for (size_t i = 0; i < numOfAtoms; i++)
	{
		vec3 color = vec3(1.0f, 1.0f, 1.0f);
		color = mPDB->getAtoms()[i]->getColor();
		matColors.push_back(color);
	}

	// Create and array Buffer to store all colors 
	mInstanceColorVbo = gl::Vbo::create(GL_ARRAY_BUFFER, matColors.size() * sizeof(vec3), matColors.data(), GL_STATIC_DRAW);

	// Setup the buffer to contain space for all vec. Each vec needs 3 floats
	geom::BufferLayout instanceColorDataLayout;
	instanceColorDataLayout.append(geom::Attrib::CUSTOM_1, 3, sizeof(vec3), 0, 1);

	// Add buffer to mesh and create batch
	mVboMesh->appendVbo(instanceColorDataLayout, mInstanceColorVbo);


	// ---------------------------------------------
	// Create BATCH
	// ---------------------------------------------
	mBatch = gl::Batch::create(mVboMesh, mShader, { {geom::CUSTOM_1, "iColor"} , { geom::CUSTOM_0, "iModelMatrix" } });
	mBatchTest = gl::Batch::create(mVboMesh, mShaderTest, { { geom::CUSTOM_1, "iColor" } ,{ geom::CUSTOM_0, "iModelMatrix" } });
}

void ProteinApp::renderToFBO()
{
	mFboDepthMap->bindFramebuffer();
	// clear out the FBO with blue
	gl::clear(Color(0.0, 0.0f, 0.0f));

	// set viewport for this FBO
	gl::ScopedViewport scpViewPort(ivec2(0.0f), mFboDepthMap->getSize());

	// Set our matrices to Cam
	gl::setMatrices(mLight.cam);

	// Render our cube
	// Draw Instances
	gl::pushModelMatrix();
	if (mVboMesh && mShader && mInstanceDataVbo)
	{
		// Number of Instances = number of atoms in pdb
		unsigned int numOfAtoms = mPDB->getAtoms().size();
		gl::scale(vec3(1.05f));
		gl::ScopedGlslProg shader(mShader);
		mBatch->drawInstanced(numOfAtoms);
	}
	gl::popModelMatrix();

	// Unbind FBO
	mFboDepthMap->unbindFramebuffer();
}

void ProteinApp::renderToTestFbo()
{
	gl::ScopedFramebuffer scopedFbo(mFboTest);
	gl::clear(Color::black());

	// setup the viewport to match the dimensions of the FBO
	gl::ScopedViewport scpVp(ivec2(0), mFboTest->getSize());

	gl::setMatrices(mCamera);

	// render the color cube
	gl::pushModelMatrix();
	if (mVboMesh && mShader && mInstanceDataVbo)
	{
		// Number of Instances = number of atoms in pdb
		unsigned int numOfAtoms = mPDB->getAtoms().size();
		gl::scale(vec3(1.05f));
		gl::ScopedGlslProg shader(mShaderTest);
		mBatchTest->drawInstanced(numOfAtoms);
	}
	gl::popModelMatrix();
}

bool ProteinApp::pickByColor(const ivec2 & position)
{
	if (!mFboTest) return false;

	// first, specify a small region around the current cursor position 
	float scaleX = mFboTest->getWidth() / (float)getWindowWidth();
	float scaleY = mFboTest->getHeight() / (float)getWindowHeight();
	ivec2 pixel((int)(position.x * scaleX), (int)((getWindowHeight() - position.y) * scaleY));
	Area area(pixel.x - 2, pixel.y - 2, pixel.x + 2, pixel.y + 2);

	// next, we need to copy this region to a non-anti-aliased framebuffer
	//  because sadly we can not sample colors from an anti-aliased one. However,
	//  this also simplifies the glReadPixels statement, so no harm done.
	//  Here, we create that non-AA buffer if it does not yet exist.
	if (!mFboTestPicking) {
		gl::Fbo::Format fmt;

		// make sure the framebuffer is not anti-aliased
		fmt.setSamples(0);
		fmt.setCoverageSamples(0);

		mFboTestPicking = gl::Fbo::create(area.getWidth(), area.getHeight(), fmt.colorTexture());
	}

	// (Cinder does not yet provide a way to handle multiple color targets in the blitTo function, 
	//  so we have to make sure the correct target is selected before calling it)
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, mFboTest->getId());
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, mFboTestPicking->getId());
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

	mFboTest->blitTo(mFboTestPicking, area, mFboTestPicking->getBounds());

	// bind the picking framebuffer, so we can read its pixels
	mFboTestPicking->bindFramebuffer();

	// read pixel value(s) in the area
	GLubyte buffer[400]; // make sure this is large enough to hold 4 bytes for every pixel!

	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glReadPixels(0, 0, mFboTestPicking->getWidth(), mFboTestPicking->getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, (void*)buffer);

	// unbind the picking framebuffer
	mFboTestPicking->unbindFramebuffer();

	// calculate the total number of pixels
	unsigned int total = (mFboTestPicking->getWidth() * mFboTestPicking->getHeight());

	// now that we have the color information, count each occuring color
	unsigned int color;

	std::map<unsigned int, unsigned int> occurences;
	for (size_t i = 0; i < total; ++i) {
		color = utils::charToInt(buffer[(i * 4) + 0], buffer[(i * 4) + 1], buffer[(i * 4) + 2]); // Here is the problem
		occurences[color]++;
	}

	// find the most occuring color by calling std::max_element using a custom comparator.
	unsigned int max = 0;

	auto itr = std::max_element(
		occurences.begin(), occurences.end(),
		[](const std::pair<unsigned int, unsigned int> &a, const std::pair<unsigned int, unsigned int> &b) {
		return a.second < b.second;
	});

	if (itr != occurences.end()) {
		color = itr->first;
		max = itr->second;
	}

	if (max >= (total / 2)) {
		int index = (color - 1);
		if (mPDB->getAtoms()[index] != nullptr)
		{
			if (mPicked.find(index) == mPicked.end())
				mPicked.insert(make_pair(index, true));
			else
				mPicked.find(index)->second = !mPicked.find(index)->second;
			return true;
		}		
	}
}

void ProteinApp::mouseMove(MouseEvent event)
{
}

void ProteinApp::mouseDown(MouseEvent event)
{
	if (event.isShiftDown())
		pickByColor(ivec2(event.getX(), event.getY()));
	else
		mCameraUi.mouseDown(event);
}

void ProteinApp::mouseDrag(MouseEvent event)
{
	if(!event.isShiftDown()) mCameraUi.mouseDrag(event);
}

void ProteinApp::mouseUp(MouseEvent event)
{
}

void ProteinApp::keyDown(KeyEvent event)
{
	switch (event.getCode()) {
	case KeyEvent::KEY_ESCAPE:
		quit();
		break;
	case KeyEvent::KEY_f:
		// toggle full screen
		setFullScreen(!isFullScreen());
		break;
	case KeyEvent::KEY_v:
		gl::enableVerticalSync(!gl::isVerticalSyncEnabled());
		break;
	}
}

void ProteinApp::keyUp(KeyEvent event)
{
}


CINDER_APP( ProteinApp, RendererGl(RendererGl::Options().msaa(8)), &ProteinApp::prepare )
