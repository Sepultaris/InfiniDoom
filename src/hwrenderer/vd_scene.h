/*
** vd_scene.h
** Backend-agnostic hardware scene collection for vDoom renderers.
*/

#ifndef VDOOM_HW_SCENE_H
#define VDOOM_HW_SCENE_H

struct sector_t;
struct side_t;
struct subsector_t;

namespace vdoom
{
	struct VdHwFlatCommand
	{
		const subsector_t *Subsector;
		sector_t *PlaneSector;
		sector_t *TextureSector;
		int Plane;
		bool OtherPlane;
	};

	enum VdHwRenderFlags
	{
		VDHW_RENDERFLOOR = 1,
		VDHW_RENDERCEILING = 2,
		VDHW_RENDERALL = 7,
		VDHW_PROCESSED = 8
	};

	struct VdHwSceneStats
	{
		unsigned int Flats;
		unsigned int OtherPlanes;
		unsigned int MissingTextureCandidates;
		unsigned int VisibleSubsectors;
		unsigned int BspDepthSkips;
		unsigned int SkippedFlats;
	};

	class VdHwScene
	{
	public:
		enum
		{
			MaxFlats = 8192
		};

		VdHwScene();
		~VdHwScene();

		void Clear();
		void CollectWorld();

		const VdHwFlatCommand *GetFlats() const;
		unsigned int GetFlatCount() const;
		const VdHwSceneStats &GetStats() const;
		unsigned char GetSubsectorRenderFlags(const subsector_t *subsector) const;

	private:
		bool AddFlat(const subsector_t *subsector, sector_t *planeSector, sector_t *textureSector, int plane, bool otherPlane);
		bool AddOtherPlaneFlat(const subsector_t *subsector, sector_t *planeSector, int plane);
		bool AddSectorFlat(sector_t *planeSector, sector_t *textureSector, int plane);
		bool EnsureFlagBuffers();
		void SetSubsectorRenderFlags(const subsector_t *subsector, unsigned char flags);
		unsigned char GetSectorRenderFlags(sector_t *sector) const;
		void SetSectorRenderFlags(sector_t *sector, unsigned char flags);
		void AddSubsectorFlats(const subsector_t *subsector);
		void CollectBspFlats(void *node, unsigned int depth);
		void CollectFallbackFlats();
		void CollectVisibleFlats();
		void CollectMissingTexturePlanes();

		VdHwFlatCommand Flats[MaxFlats];
		VdHwSceneStats Stats;
		unsigned int FlatCount;
		unsigned char *SubsectorRenderFlags;
		unsigned int SubsectorRenderFlagCount;
		unsigned char *SectorRenderFlags;
		unsigned int SectorRenderFlagCount;
	};
}

#endif
