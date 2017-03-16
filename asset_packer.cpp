#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <vector>
#include <string>
#include <map>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <ft2build.h>
#include FT_FREETYPE_H  
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <experimental/optional>

namespace std {
	using std::experimental::optional;
	using std::experimental::make_optional;
	using std::experimental::nullopt;
}

#include "asset_ids.h"

struct Model_Vertex {
	float position[3];
	float normal[3];
	float uv[2];
};

struct Raw_Mesh_Info {
	Raw_Mesh_Info(uint32_t ni, uint32_t bv, std::optional<std::string> dp, std::optional<std::string> sp) : num_indices(ni), base_vertex(bv), diffuse_path(dp), specular_path(sp) {}
	uint32_t num_indices;
	uint32_t base_vertex;
	std::optional<std::string> diffuse_path;
	std::optional<std::string> specular_path;
};

struct Raw_Model {
	std::vector<Raw_Mesh_Info> raw_mesh_infos;
	std::vector<Model_Vertex> vertices;
	std::vector<uint32_t> indices;
};

static void
process_assimp_node(aiNode* node, const aiScene* scene, Raw_Model *rm)
{
	for (unsigned m = 0; m < node->mNumMeshes; ++m) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[m]];
		// Assimp keeps track of indices per mesh, but we want them per model.
		unsigned base_index = rm->vertices.size();
		assert(mesh->HasPositions() && mesh->HasNormals() && mesh->HasFaces());
		for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
			Model_Vertex v;
			v.position[0] = mesh->mVertices[i].x;
			v.position[1] = mesh->mVertices[i].y;
			v.position[2] = mesh->mVertices[i].z;
			if (mesh->mNormals) {
				v.normal[0] = mesh->mNormals[i].x;
				v.normal[1] = mesh->mNormals[i].y;
				v.normal[2] = mesh->mNormals[i].z;
			}
			if (mesh->mTextureCoords[0]) {
				v.uv[0] = mesh->mTextureCoords[0][i].x;
				v.uv[1] = mesh->mTextureCoords[0][i].y;
			}
			rm->vertices.push_back(v);
		}
		int base_vertex = rm->indices.size();
		int num_mesh_inds = 0;
		for (unsigned i = 0; i < mesh->mNumFaces; ++i) {
			aiFace face = mesh->mFaces[i];
			assert(face.mNumIndices == 3);
			num_mesh_inds += face.mNumIndices;
			for (unsigned j = 0; j < face.mNumIndices; ++j) {
				rm->indices.push_back(base_index + face.mIndices[j]);
			}
		}
		// Doesn't handle multiple textures per mesh.
		aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
		aiString diffuse_path, specular_path;
		aiReturn has_diffuse = mat->GetTexture(aiTextureType_DIFFUSE, 0, &diffuse_path);
		aiReturn has_specular = mat->GetTexture(aiTextureType_SPECULAR, 0, &specular_path);
		if (has_diffuse == AI_SUCCESS && has_specular == AI_SUCCESS)
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, diffuse_path.C_Str(), specular_path.C_Str());
		else if (has_diffuse == AI_SUCCESS)
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, diffuse_path.C_Str(), std::nullopt);
		else if (has_specular == AI_SUCCESS)
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, std::nullopt, specular_path.C_Str());
		else
			rm->raw_mesh_infos.emplace_back(num_mesh_inds, base_vertex, std::nullopt, std::nullopt);
	}
	for (unsigned i = 0; i < node->mNumChildren; ++i)
		process_assimp_node(node->mChildren[i], scene, rm);
}

struct ID_To_Filename {
	int id;
	const char *filename;
};

ID_To_Filename model_map[] = {
	{ NANOSUIT_MODEL, "nanosuit.obj"},
};

std::map<std::string, uint32_t> texture_map;

// TODO: This won't work for large file (>2GB). ftell might not work properly with file larger than 2GB because it returns a signed long as the offset.
// Might have to use some OS specific offset query function.
int
main(int argc, char **argv)
{
	uint32_t num_textures_in_file = 0;
	FILE *file = fopen("assets.ahh", "wb");
	fseek(file, sizeof(Asset_File_Header), SEEK_CUR);

	struct Asset_File_Header header;

	// Write models.
	int num_assets = sizeof(model_map)/sizeof(model_map[0]);
	Raw_Model rm;
	std::string model_prepath = "assets/models/";
	for (int i = 0; i < num_assets; ++i) {
		std::string path = model_prepath + model_map[i].filename;
		Assimp::Importer import;
		const aiScene* scene = import.ReadFile(path.c_str(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			printf("Assimp error: %s", import.GetErrorString());
			continue;
		}
		process_assimp_node(scene->mRootNode, scene, &rm);
		header.named_asset_offsets[model_map[i].id] = ftell(file);

		uint32_t num_verts = rm.vertices.size(), num_inds = rm.indices.size(), num_meshes = rm.raw_mesh_infos.size(), no_tex = (uint32_t) -1;
		printf("%d %d %d\n", num_verts, num_inds, num_meshes);
		fwrite(&num_verts, sizeof(uint32_t), 1, file);
		fwrite(&num_inds, sizeof(uint32_t), 1, file);
		fwrite(rm.vertices.data(), sizeof(Model_Vertex), rm.vertices.size(), file);
		fwrite(rm.indices.data(), sizeof(unsigned), rm.indices.size(), file);
		fwrite(&num_meshes, sizeof(uint32_t), 1, file);
		for (size_t j = 0; j < num_meshes; ++j) {
			fwrite(&rm.raw_mesh_infos[j].num_indices, sizeof(uint32_t), 1, file);
			fwrite(&rm.raw_mesh_infos[j].base_vertex, sizeof(uint32_t), 1, file);
			printf("%d %d\n", rm.raw_mesh_infos[j].num_indices, rm.raw_mesh_infos[j].base_vertex);
			// TODO: Loop over all the textures we want to check for.
			if (rm.raw_mesh_infos[j].specular_path) {
				printf("Using specular texture %s\n", rm.raw_mesh_infos[j].specular_path->c_str());
				auto it = texture_map.find(*rm.raw_mesh_infos[j].specular_path);
				if (it == texture_map.end()) {
					texture_map[*rm.raw_mesh_infos[j].specular_path] = num_textures_in_file;
					fwrite(&num_textures_in_file, sizeof(uint32_t), 1, file);
					++num_textures_in_file;
				} else
					fwrite(&it->second, sizeof(uint32_t), 1, file);
			} else // No texture available.
				fwrite(&no_tex, sizeof(uint32_t), 1, file);
			if (rm.raw_mesh_infos[j].diffuse_path) {
				printf("Using diffuse texture %s\n", rm.raw_mesh_infos[j].diffuse_path->c_str());
				auto it = texture_map.find(*rm.raw_mesh_infos[j].diffuse_path);
				if (it == texture_map.end()) {
					texture_map[*rm.raw_mesh_infos[j].diffuse_path] = num_textures_in_file;
					fwrite(&num_textures_in_file, sizeof(uint32_t), 1, file);
					++num_textures_in_file;
				} else
					fwrite(&it->second, sizeof(uint32_t), 1, file);
			} else // No texture available.
				fwrite(&no_tex, sizeof(uint32_t), 1, file);
		}
		rm.vertices.clear();
		rm.indices.clear();
		rm.raw_mesh_infos.clear();
	}
	printf("num tex in file %d %d\n", num_textures_in_file, texture_map.size());
	header.num_mesh_textures = num_textures_in_file;

	// Write textures.
	header.mesh_texture_table_offset = ftell(file);
	fpos_t tex_table_pos;
	fgetpos(file, &tex_table_pos);
	uint32_t *tex_table_header = (uint32_t *)malloc(num_textures_in_file * sizeof(uint32_t));
	fseek(file, num_textures_in_file * sizeof(uint32_t), SEEK_CUR);
	for (auto itr : texture_map) {
		SDL_Surface *tex;
		if (!(tex = IMG_Load(itr.first.c_str()))) {
			printf("IMG_Load failed! IMG_GetError: %s\n", IMG_GetError());
			tex_table_header[itr.second] = 0;
			continue;
		}
		tex_table_header[itr.second] = ftell(file);
		fwrite(&tex->format->BytesPerPixel, sizeof(tex->format->BytesPerPixel), 1, file);
		fwrite(&tex->w, sizeof(tex->w), 1, file);
		fwrite(&tex->h, sizeof(tex->h), 1, file);
		fwrite(&tex->pitch, sizeof(tex->pitch), 1, file);
		fwrite(tex->pixels, tex->pitch * tex->h, 1, file);
	}

	// Write tex table header.
	fsetpos(file, &tex_table_pos);
	fwrite(tex_table_header, sizeof(uint32_t), num_textures_in_file, file);

	// Write asset file header.
	fseek(file, 0, SEEK_SET);
	fwrite(&header, sizeof(Asset_File_Header), 1, file);

	free(tex_table_header);
}
