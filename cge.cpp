#include <GL/freeglut.h>
#include <stdio.h>
#include "render.h"

namespace cge {

void
init()
{
	render::init_gl();
}

} //namspace cge

int
main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA);
	glutInitWindowSize(800, 600);
	glutInitContextVersion(3, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutCreateWindow(argv[0]);
	glutCreateWindow("GLEW Test");
	cge::init();
	glutDisplayFunc(render::display);
	glutMainLoop();
}
