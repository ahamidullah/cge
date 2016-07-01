#ifndef __RENDER_H__
#define __RENDER_H__

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace render {
	void init(const int screen_widht, const int screen_height);
	void render(glm::mat4 view);
}

#endif
