/*
** vd_scene.cpp
** Backend-agnostic hardware scene collection for vDoom renderers.
*/

#include <string.h>

#include "hwrenderer/vd_scene.h"

#include "m_fixed.h"
#include "r_state.h"
#include "textures/textures.h"

namespace vdoom
{
	VdHwScene::VdHwScene()
	{
		Clear();
	}

	void VdHwScene::Clear()
	{
		memset(Flats, 0, sizeof(Flats));
		memset(&Stats, 0, sizeof(Stats));
		FlatCount = 0;
	}

	const VdHwFlatCommand *VdHwScene::GetFlats() const
	{
		return Flats;
	}

	unsigned int VdHwScene::GetFlatCount() const
	{
		return FlatCount;
	}

	const VdHwSceneStats &VdHwScene::GetStats() const
	{
		return Stats;
	}

	bool VdHwScene::AddFlat(const subsector_t *subsector, sector_t *planeSector, sector_t *textureSector, int plane, bool otherPlane)
	{
		if (subsector == NULL || planeSector == NULL || textureSector == NULL)
		{
			++Stats.SkippedFlats;
			return false;
		}
		if (FlatCount >= MaxFlats)
		{
			++Stats.SkippedFlats;
			return false;
		}

		VdHwFlatCommand &command = Flats[FlatCount++];
		command.Subsector = subsector;
		command.PlaneSector = planeSector;
		command.TextureSector = textureSector;
		command.Plane = plane;
		command.OtherPlane = otherPlane;
		++Stats.Flats;
		if (otherPlane)
		{
			++Stats.OtherPlanes;
		}
		return true;
	}

	bool VdHwScene::AddOtherPlaneFlat(const subsector_t *subsector, sector_t *planeSector, int plane)
	{
		if (subsector == NULL || planeSector == NULL)
		{
			++Stats.SkippedFlats;
			return false;
		}

		for (unsigned int i = 0; i < FlatCount; ++i)
		{
			if (Flats[i].OtherPlane &&
				Flats[i].Subsector == subsector &&
				Flats[i].PlaneSector == planeSector &&
				Flats[i].TextureSector == planeSector &&
				Flats[i].Plane == plane)
			{
				return false;
			}
		}
		return AddFlat(subsector, planeSector, planeSector, plane, true);
	}

	static bool SideTextureMissing(side_t *side, int texturePart)
	{
		if (side == NULL)
		{
			return true;
		}
		const FTextureID textureId = side->GetTexture(texturePart);
		FTexture *texture = textureId.isValid() ? TexMan[textureId] : NULL;
		return texture == NULL || texture->UseType == FTexture::TEX_Null;
	}

	void VdHwScene::CollectSectorFlats()
	{
		if (sectors == NULL || numsectors <= 0)
		{
			return;
		}

		for (int i = 0; i < numsectors; ++i)
		{
			sector_t *sector = &sectors[i];
			if (sector->subsectors == NULL || sector->subsectorcount <= 0)
			{
				continue;
			}

			for (int j = 0; j < sector->subsectorcount; ++j)
			{
				const subsector_t *subsector = sector->subsectors[j];
				if (subsector == NULL)
				{
					++Stats.SkippedFlats;
					continue;
				}
				AddFlat(subsector, sector, sector, sector_t::floor, false);
				AddFlat(subsector, sector, sector, sector_t::ceiling, false);
			}
		}
	}

	void VdHwScene::CollectMissingTexturePlanes()
	{
		if (subsectors == NULL || numsubsectors <= 0)
		{
			return;
		}

		for (int i = 0; i < numsubsectors; ++i)
		{
			const subsector_t *subsector = &subsectors[i];
			if (subsector->firstline == NULL || subsector->numlines < 3)
			{
				++Stats.SkippedFlats;
				continue;
			}

			for (unsigned int j = 0; j < subsector->numlines; ++j)
			{
				const seg_t *seg = &subsector->firstline[j];
				if (seg == NULL || seg->v1 == NULL || seg->v2 == NULL || seg->frontsector == NULL ||
					seg->backsector == NULL || seg->sidedef == NULL)
				{
					continue;
				}

				const double frontFloor1 = FIXED2FLOAT(seg->frontsector->floorplane.ZatPoint(seg->v1));
				const double frontFloor2 = FIXED2FLOAT(seg->frontsector->floorplane.ZatPoint(seg->v2));
				const double frontCeiling1 = FIXED2FLOAT(seg->frontsector->ceilingplane.ZatPoint(seg->v1));
				const double frontCeiling2 = FIXED2FLOAT(seg->frontsector->ceilingplane.ZatPoint(seg->v2));
				const double backFloor1 = FIXED2FLOAT(seg->backsector->floorplane.ZatPoint(seg->v1));
				const double backFloor2 = FIXED2FLOAT(seg->backsector->floorplane.ZatPoint(seg->v2));
				const double backCeiling1 = FIXED2FLOAT(seg->backsector->ceilingplane.ZatPoint(seg->v1));
				const double backCeiling2 = FIXED2FLOAT(seg->backsector->ceilingplane.ZatPoint(seg->v2));

				if ((frontCeiling1 > backCeiling1 || frontCeiling2 > backCeiling2) &&
					SideTextureMissing(seg->sidedef, side_t::top))
				{
					++Stats.MissingTextureCandidates;
					AddOtherPlaneFlat(subsector, seg->backsector, sector_t::ceiling);
				}
				if ((backFloor1 > frontFloor1 || backFloor2 > frontFloor2) &&
					SideTextureMissing(seg->sidedef, side_t::bottom))
				{
					++Stats.MissingTextureCandidates;
					AddOtherPlaneFlat(subsector, seg->backsector, sector_t::floor);
				}
			}
		}
	}

	void VdHwScene::CollectWorld()
	{
		Clear();
		CollectSectorFlats();
		CollectMissingTexturePlanes();
	}
}
