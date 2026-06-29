/*
** vd_scene.cpp
** Backend-agnostic hardware scene collection for vDoom renderers.
*/

#include <string.h>

#include "hwrenderer/vd_scene.h"

#include "m_fixed.h"
#include "r_state.h"
#include "r_utility.h"
#include "textures/textures.h"

namespace vdoom
{
	enum
	{
		MaxBspDepth = 256
	};

	VdHwScene::VdHwScene()
		: SubsectorRenderFlags(NULL), SubsectorRenderFlagCount(0),
		  SectorRenderFlags(NULL), SectorRenderFlagCount(0)
	{
		Clear();
	}

	VdHwScene::~VdHwScene()
	{
		Clear();
	}

	void VdHwScene::Clear()
	{
		delete[] SubsectorRenderFlags;
		SubsectorRenderFlags = NULL;
		SubsectorRenderFlagCount = 0;
		delete[] SectorRenderFlags;
		SectorRenderFlags = NULL;
		SectorRenderFlagCount = 0;
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

	unsigned char VdHwScene::GetSubsectorRenderFlags(const subsector_t *subsector) const
	{
		if (subsector == NULL || SubsectorRenderFlags == NULL ||
			subsector < subsectors || subsector >= subsectors + SubsectorRenderFlagCount)
		{
			return 0;
		}
		return SubsectorRenderFlags[subsector - subsectors];
	}

	bool VdHwScene::EnsureFlagBuffers()
	{
		if (numsubsectors > 0 && SubsectorRenderFlags == NULL)
		{
			SubsectorRenderFlagCount = (unsigned int)numsubsectors;
			SubsectorRenderFlags = new unsigned char[SubsectorRenderFlagCount];
			memset(SubsectorRenderFlags, 0, SubsectorRenderFlagCount);
		}
		if (numsectors > 0 && SectorRenderFlags == NULL)
		{
			SectorRenderFlagCount = (unsigned int)numsectors;
			SectorRenderFlags = new unsigned char[SectorRenderFlagCount];
			memset(SectorRenderFlags, 0, SectorRenderFlagCount);
		}
		return true;
	}

	void VdHwScene::SetSubsectorRenderFlags(const subsector_t *subsector, unsigned char flags)
	{
		if (subsector == NULL || SubsectorRenderFlags == NULL ||
			subsector < subsectors || subsector >= subsectors + SubsectorRenderFlagCount)
		{
			return;
		}
		SubsectorRenderFlags[subsector - subsectors] |= flags;
	}

	unsigned char VdHwScene::GetSectorRenderFlags(sector_t *sector) const
	{
		if (sector == NULL || SectorRenderFlags == NULL ||
			sector < sectors || sector >= sectors + SectorRenderFlagCount)
		{
			return 0;
		}
		return SectorRenderFlags[sector - sectors];
	}

	void VdHwScene::SetSectorRenderFlags(sector_t *sector, unsigned char flags)
	{
		if (sector == NULL || SectorRenderFlags == NULL ||
			sector < sectors || sector >= sectors + SectorRenderFlagCount)
		{
			return;
		}
		SectorRenderFlags[sector - sectors] |= flags;
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

	bool VdHwScene::AddSectorFlat(sector_t *planeSector, sector_t *textureSector, int plane)
	{
		if (planeSector == NULL || textureSector == NULL)
		{
			++Stats.SkippedFlats;
			return false;
		}

		const unsigned char renderFlag = plane == sector_t::floor ? VDHW_RENDERFLOOR : VDHW_RENDERCEILING;
		if (GetSectorRenderFlags(textureSector) & renderFlag)
		{
			return false;
		}
		SetSectorRenderFlags(textureSector, renderFlag);

		if (FlatCount >= MaxFlats)
		{
			++Stats.SkippedFlats;
			return false;
		}

		VdHwFlatCommand &command = Flats[FlatCount++];
		command.Subsector = NULL;
		command.PlaneSector = planeSector;
		command.TextureSector = textureSector;
		command.Plane = plane;
		command.OtherPlane = false;
		++Stats.Flats;
		return true;
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

	void VdHwScene::AddSubsectorFlats(const subsector_t *subsector)
	{
		if (subsector == NULL || subsector->firstline == NULL || subsector->numlines < 3)
		{
			++Stats.SkippedFlats;
			return;
		}
		if (subsector->flags & SSECF_DEGENERATE)
		{
			++Stats.SkippedFlats;
			return;
		}

		sector_t *planeSector = subsector->sector;
		sector_t *textureSector = subsector->render_sector != NULL ? subsector->render_sector : planeSector;
		const double viewX = FIXED2FLOAT(viewx);
		const double viewY = FIXED2FLOAT(viewy);
		const double viewZ = FIXED2FLOAT(viewz);
		SetSubsectorRenderFlags(subsector, VDHW_PROCESSED | VDHW_RENDERALL);
		if (planeSector->floorplane.ZatPoint(viewX, viewY) <= viewZ)
		{
			AddSectorFlat(planeSector, textureSector, sector_t::floor);
		}
		if (planeSector->ceilingplane.ZatPoint(viewX, viewY) >= viewZ)
		{
			AddSectorFlat(planeSector, textureSector, sector_t::ceiling);
		}
		++Stats.VisibleSubsectors;
	}

	void VdHwScene::CollectBspFlats(void *node, unsigned int depth)
	{
		if (node == NULL || FlatCount >= MaxFlats)
		{
			return;
		}
		if (depth > MaxBspDepth)
		{
			++Stats.BspDepthSkips;
			return;
		}

		while (!((size_t)node & 1))
		{
			const node_t *bsp = static_cast<const node_t *>(node);
			const int side = R_PointOnSide(viewx, viewy, bsp);
			CollectBspFlats(bsp->children[side], depth + 1);
			if (FlatCount >= MaxFlats)
			{
				return;
			}
			node = bsp->children[side ^ 1];
			if (node == NULL)
			{
				return;
			}
			if (++depth > MaxBspDepth)
			{
				++Stats.BspDepthSkips;
				return;
			}
		}

		const subsector_t *subsector = reinterpret_cast<const subsector_t *>(reinterpret_cast<const BYTE *>(node) - 1);
		AddSubsectorFlats(subsector);
	}

	void VdHwScene::CollectFallbackFlats()
	{
		if (subsectors == NULL || numsubsectors <= 0)
		{
			return;
		}

		const double camX = FIXED2FLOAT(viewx);
		const double camY = FIXED2FLOAT(viewy);
		const double maxDistance = 4096.0;
		const double maxDistanceSquared = maxDistance * maxDistance;

		for (int i = 0; i < numsubsectors && FlatCount < MaxFlats; ++i)
		{
			const subsector_t *subsector = &subsectors[i];
			if (subsector->firstline == NULL || subsector->numlines < 3)
			{
				++Stats.SkippedFlats;
				continue;
			}

			double centerX = 0.0;
			double centerY = 0.0;
			unsigned int centerPoints = 0;
			for (unsigned int j = 0; j < subsector->numlines; ++j)
			{
				const vertex_t *vertex = subsector->firstline[j].v1;
				if (vertex != NULL)
				{
					centerX += FIXED2FLOAT(vertex->x);
					centerY += FIXED2FLOAT(vertex->y);
					++centerPoints;
				}
			}
			if (centerPoints == 0)
			{
				++Stats.SkippedFlats;
				continue;
			}
			centerX /= (double)centerPoints;
			centerY /= (double)centerPoints;

			const double dx = centerX - camX;
			const double dy = centerY - camY;
			if (dx * dx + dy * dy <= maxDistanceSquared)
			{
				AddSubsectorFlats(subsector);
			}
			else
			{
				++Stats.SkippedFlats;
			}
		}
	}

	void VdHwScene::CollectVisibleFlats()
	{
		if (subsectors == NULL || numsubsectors <= 0)
		{
			return;
		}
		if (nodes != NULL && numnodes > 0)
		{
			CollectBspFlats(nodes + numnodes - 1, 0);
		}
		else
		{
			CollectFallbackFlats();
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
		EnsureFlagBuffers();
		CollectVisibleFlats();
		CollectMissingTexturePlanes();
	}
}
