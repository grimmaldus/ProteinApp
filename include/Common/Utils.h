#include <string>
#include "cinder/CinderGlm.h"

namespace utils
{

inline static std::string remove_extension(const std::string& filename) {
	size_t lastdot = filename.find_last_of(".");
	if (lastdot == std::string::npos) return filename;
	return filename.substr(0, lastdot);
}

inline static glm::vec3 charToColor(unsigned char r, unsigned char g, unsigned char b)
{
	return glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f);
};

inline static unsigned int charToInt(unsigned char r, unsigned char g, unsigned char b)
{
	return b + (g << 8) + (r << 16);
};

inline static glm::vec3 intToColor(unsigned int i)
{
	unsigned char r = (i >> 16) & 0xFF;
	unsigned char g = (i >> 8) & 0xFF;
	unsigned char b = (i >> 0) & 0xFF;
	return glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f);
};

inline static unsigned int colorToInt(const glm::vec3 &color)
{
	unsigned char r = (unsigned char)(color.r * 255);
	unsigned char g = (unsigned char)(color.g * 255);
	unsigned char b = (unsigned char)(color.b * 255);
	return b + (g << 8) + (r << 16);
};



}
