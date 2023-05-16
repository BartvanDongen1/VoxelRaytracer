#pragma once
#include <vector>
#include <glm/vec3.hpp>

struct VoxelAtlasItem
{
	glm::vec3 color;
	float padding;
};

class VoxelAtlas
{
public:
	VoxelAtlas() {};
	~VoxelAtlas() {};

	void addItem(const VoxelAtlasItem &aItem);
	void clearItems();

	const VoxelAtlasItem* getItems() const;
	size_t getItemCount() const;
private:
	std::vector<VoxelAtlasItem> items;
};

