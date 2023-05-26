#pragma once

class Texture
{
public:
	void* textureData{ 0 };

	int textureWidth{ 0 };
	int textureHeight{ 0 };
	int bytesPerPixel{ 0 };
};