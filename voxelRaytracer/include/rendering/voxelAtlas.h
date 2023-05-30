#pragma once
#include <vector>
#include <glm/vec4.hpp>

struct VoxelAtlasItem
{
	glm::vec4 colorAndRoughness;
	glm::vec4 specularAndPercent;

	int isLight{ 0 };
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

