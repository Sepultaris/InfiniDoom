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

	struct VdHwSceneStats
	{
		unsigned int Flats;
		unsigned int OtherPlanes;
		unsigned int MissingTextureCandidates;
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

		void Clear();
		void CollectWorld();

		const VdHwFlatCommand *GetFlats() const;
		unsigned int GetFlatCount() const;
		const VdHwSceneStats &GetStats() const;

	private:
		bool AddFlat(const subsector_t *subsector, sector_t *planeSector, sector_t *textureSector, int plane, bool otherPlane);
		bool AddOtherPlaneFlat(const subsector_t *subsector, sector_t *planeSector, int plane);
		void CollectSectorFlats();
		void CollectMissingTexturePlanes();

		VdHwFlatCommand Flats[MaxFlats];
		VdHwSceneStats Stats;
		unsigned int FlatCount;
	};
}

#endif
