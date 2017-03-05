#ifndef __ASSET_IDS_H__
#define __ASSET_IDS_H__

enum Model_ID {
	NANOSUIT_MODEL = 0,

	//ARIAL_FONT,
	//ANONYMOUS_PRO_FONT,

	NUM_MODEL_IDS
};

enum Font_ID {

	NUM_FONT_IDS
};

enum Sound_ID {

	NUM_SOUND_IDS
};

enum Named_Asset_IDs {
	NUM_NAMED_ASSET_IDS = NUM_MODEL_IDS + NUM_FONT_IDS + NUM_SOUND_IDS
};

//#pragma pack(1)
struct Asset_File_Header {
	long named_asset_offsets[NUM_NAMED_ASSET_IDS];
	long num_mesh_textures;
	long mesh_texture_table_offset; // We don't give each mesh texture it's own Asset_ID since we won't be referencing it directly.
};
//#pragma pack(pop)

#endif
