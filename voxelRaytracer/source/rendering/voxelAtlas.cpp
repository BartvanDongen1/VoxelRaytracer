#include "rendering/voxelAtlas.h"

void VoxelAtlas::addItem(const VoxelAtlasItem& aItem)
{
	items.push_back(aItem);
}

void VoxelAtlas::clearItems()
{
	items.clear();
}

const VoxelAtlasItem* VoxelAtlas::getItems() const
{
	if (items.size() == 0) return nullptr;

	return &items[0];
}

size_t VoxelAtlas::getItemCount() const
{
	return items.size();
}
