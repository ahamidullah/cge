//#include "zlib.h"
//#include "fbx_loader.h"
//#include <stdio.h>
//#include <ctype.h>
//#include "string.h"
//
//// hacky fbx loader
//
///*      Layout
// *	4 	Uint32 	EndOffset
// *	4 	Uint32 	NumProperties
// *	4 	Uint32 	PropertyListLen
// *	1 	Uint8t 	NameLen
// *	NameLen char 	Name
// *	? 	? 	Property[n], for n in 0:PropertyListLen
// *	Optional 		
// *	? 	? 	NestedList
// *	13 	uint8[] NULL-record
// */
//
//const int short_sz = 2;
//const int int_sz = 4;
//const int char_sz = 1;
//const int float_sz = 4;
//
//template <typename T>
//T
//read_and_move_ptr(const char **ptr)
//{
//	T val = *((T*) *ptr);
//	*ptr += sizeof(T);
//	return val;
//}
//
////void
////get_properties(uint32_t num_props, const char *data, const char *fname) {
////	for (uint32_t i = 0; i < num_props; ++i) {
////		char prop_type = *data++;
////		if (isupper(prop_type)) { // primitive type
////			switch (prop_type) {
////				case 'Y': { // short
////					int16_t y = read_and_move_ptr<int16_t>(&data);
////					break;
////				}
////				case 'C': { // bit bool
////					uint8_t c = read_and_move_ptr<uint8_t>(&data);
////					break;
////				}
////				case 'I': { // signed int
////					int32_t i = read_and_move_ptr<int32_t>(&data);
////					break;
////				}
////				case 'F': { // float
////					float f = read_and_move_ptr<float>(&data);
////					break;
////				}
////				case 'D': { // double
////					double d = read_and_move_ptr<double>(&data);
////					break;
////				}
////				case 'L': { // long
////					int64_t l = read_and_move_ptr<int64_t>(&data);
////					break;
////				}
////				default: {
////					z_log_err("unrecognized property type \'%c\' in fbx file %s", prop_type, fname);
////					break;
////				}
////			}
////		} else { // array type
////			uint32_t array_len = read_and_move_ptr<uint32_t>(&data);
////			uint32_t encoding = read_and_move_ptr<uint32_t>(&data);
////			uint32_t compressed_len = read_and_move_ptr<uint32_t>(&data);
////			if (encoding) {
////				z_log_err("fbx file %s has encoded data");
////				return; // Z_Maybe<Z_Array<Model_Info>>(Z_Nothing());
////			}
////			switch (prop_type) {
////				case 'y': { // short
////					int16_t y = read_and_move_ptr<int16_t>(&data);
////					break;
////				}
////				case 'c': { // bit bool
////					uint8_t c = read_and_move_ptr<int8_t>(&data);
////					break;
////				}
////				case 'i': { // signed int
////					int32_t i = read_and_move_ptr<int32_t>(&data);
////					break;
////				}
////				case 'f': { // float
////					float f = read_and_move_ptr<float>(&data);
////					break;
////				}
////				case 'd': { // double
////					double d = read_and_move_ptr<double>(&data);
////					break;
////				}
////				case 'l': { // long
////					int64_t l = read_and_move_ptr<int64_t>(&data);
////					break;
////				}
////				default: {
////					z_log_err("unrecognized property type \'%c\' in fbx file %s", prop_type, fname);
////					break;
////				}
////			}
////		}
////	}
////}
//
//// NOTE: do we have to worry about endianness?
////static Z_Maybe<Z_Array<Model_Info>>
//Z_Array<GLfloat>
//parse_node(const char *file_start, const char *fbx_data, const char *fname)
//{
//	uint32_t node_len = read_and_move_ptr<uint32_t>(&fbx_data);
//	uint32_t num_properties = read_and_move_ptr<uint32_t>(&fbx_data);
//	uint32_t property_list_len = read_and_move_ptr<uint32_t>(&fbx_data);
//	uint8_t name_len = read_and_move_ptr<uint8_t>(&fbx_data);
//	char *name = z_malloc<char>(name_len+1);
//	for (uint32_t i = 0; i < name_len; ++i)
//		name[i] = *fbx_data++;
//	name[name_len] = '\0';
//	printf("%s\n", name);
//	printf("%d\n", num_properties);
//	if (!strcmp(name, "Objects")) {
//		z_free(name);
//		return parse_node(file_start, fbx_data + property_list_len, fname);
//	} else if (!strcmp(name, "Model")) {
//		z_free(name);
//		return parse_node(file_start, fbx_data + property_list_len, fname);
//	} else if (!strcmp(name, "Vertices")) {
//		z_free(name);
//		auto verts = Z_Array<GLfloat>::make(num_properties);
//		for (uint32_t i = 0; i < num_properties; ++i)
//			verts.push(read_and_move_ptr<GLfloat>(&fbx_data));
//		return verts;
//	}
//	z_free(name);
//	return parse_node(file_start, file_start+node_len, fname);
//}
//
//void
//load_model_info(const Z_Array<const char *> &fnames)
//{
//	auto load = [](const char *name) {
//		const char *data = z_load_file(name, "rb"); // read binary
//		return data;
//	};
//	auto files = z_map(fnames, load);
//	parse_node(files[0], files[0]+27, fnames[0]);
//}
