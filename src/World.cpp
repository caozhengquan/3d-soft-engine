
#include "World.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "Mat.h"
#include "Quaternion.h"
#include "Mesh.h"
#include <memory>
#include <limits>

g3::World::World(unsigned int w, unsigned int h):
width {w},
height {h},
frontBuffer {Gdk::Pixbuf::create(Gdk::Colorspace::COLORSPACE_RGB, true, 8, width, height)},
targetFrameTime {33300000},
camera { Vec3{17, 7, -20}, Vec3{2, 0, 2}, 1280 }
{
	// initializes the depth buffer.
	int depthSize = width * height;
	depthBuffer.reset(new float[depthSize]);
	std::fill(depthBuffer.get(), depthBuffer.get() + depthSize, std::numeric_limits<float>::infinity());


	// start frame time
	startFrameTime = clock_time();

	g3::loadCube(cube);
	
	// enable mouse wheel detection
	add_events(Gdk::BUTTON_PRESS_MASK | Gdk::SCROLL_MASK);

	// register idle function
	Glib::signal_idle().connect(sigc::mem_fun(*this, &World::on_idle));

}


/**
 * Clears the backbuffer.
 */
void g3::World::clear()
{
	// Fill the buffer with color white
	frontBuffer->fill(0xfafad2ff);

	// Clears the depth buffer
	std::fill(depthBuffer.get(), depthBuffer.get() + (width*height), std::numeric_limits<float>::infinity());
}

/**
 * Detects mouse wheel movements. Called by the GUI.
 */
bool g3::World::on_scroll_event(GdkEventScroll* event)
{
	float zoomFactorPercent = 0.05;

	if (event->direction == GdkScrollDirection::GDK_SCROLL_UP)
	{
		camera.zoomFactor += camera.zoomFactor * zoomFactorPercent;
	}
	else
	{
		camera.zoomFactor -= camera.zoomFactor * zoomFactorPercent;
	}

	return true;
}

/**
 * The drawing function, called by the GUI.
 */
bool g3::World::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
	
	render();
	
	// Draw the buffer
	Gdk::Cairo::set_source_pixbuf(cr, frontBuffer);
	cr->paint();

	return true;
}

/**
 * Renders the scene.
 */
void g3::World::render()
{
	clear();

	Vec3 upWorld {0,1,0};
	Mat4 viewMatrix = g3::createLookAtLHMatrix(camera.eye, camera.target, upWorld);
	Mat4 projectionMatrix = g3::createPerspectiveFovLHMatrix(0.78f, width / (float)height, 0.01f, 25.0f);
	Mat4 transformMatrix = g3::getWorldMatrix(cube) * viewMatrix * projectionMatrix;

	for (unsigned int i = 0; i < cube.nFaces; i++)
	{
		Vec3 v[3];
		int mapToWin[6];
		
		for (unsigned int j = 0; j < 3; j++)
		{
			v[j] = transformP3( cube.vertices[ cube.faces[i].vertexIndex[j] ].pos, transformMatrix );
			
			mapToWin[2*j]   = mapXToWin( v[j][0] );
			mapToWin[2*j+1] = mapYToWin( v[j][1] );
		}

		unsigned long color = createRGBA(0, 0, 128, 255);
		drawLine(mapToWin[0], mapToWin[1], v[0][2], mapToWin[2], mapToWin[3], v[1][2], color);
		drawLine(mapToWin[2], mapToWin[3], v[1][2], mapToWin[4], mapToWin[5], v[2][2], color);
		drawLine(mapToWin[4], mapToWin[5], v[2][2], mapToWin[0], mapToWin[1], v[0][2], color);
	}



	Mat4 staticMatrix = createScaleMatrix(1) * viewMatrix * projectionMatrix;
	
	// render grid ground
	float step = 1;
	int size = 12; // size X size
	Vec3 grid [ 4*(size+1) ];
	
	Vec3 startX {  size/2.0f*step, 0, size/2.0f*step  };
	for (int n = 0, m = 0; n < (size+1); n++, m+=4)
	{
		grid[m]   = { startX[0],               0, startX[2] - (n*step) };	
		grid[m+1] = { startX[0] - (size*step), 0, startX[2] - (n*step) };	
		grid[m+2] = { startX[0] - (n*step),    0, startX[2] };	
		grid[m+3] = { startX[0] - (n*step),    0, startX[2] - (size*step) };	
	}

	unsigned long gridColor = createRGBA(205, 201, 201, 255);
	for (int n = 0; n < 4*(size+1); n+=2)
	{
		Vec3 g1 = transformP3( grid[n], staticMatrix );
		int g1X = mapXToWin( g1[0] );
		int g1Y = mapYToWin( g1[1] );
		Vec3 g2 = transformP3( grid[n+1], staticMatrix );
		int g2X = mapXToWin( g2[0] );
		int g2Y = mapYToWin( g2[1] );
		drawLine(g1X, g1Y, g1[2], g2X, g2Y, g2[2], gridColor);
	}

	// render axes
	Vec3 origo {0, 0, 0};
	origo = transformP3( origo, staticMatrix );

	int origoX = mapXToWin( origo[0] );
	int origoY = mapYToWin( origo[1] );

	Vec3 axes[] { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
	unsigned long axesColor[] {
		createRGBA(255, 0, 0, 255),
		createRGBA(0, 255, 0, 255),
		createRGBA(0, 0, 255, 255)
	};

	for (int k = 0; k < 3; k++)
	{
		Vec3 axisEnd = transformP3( axes[k], staticMatrix );
		int endX = mapXToWin( axisEnd[0] );
		int endY = mapYToWin( axisEnd[1] );
		drawLine(origoX, origoY, origo[2], endX, endY, axisEnd[2], axesColor[k]);
	}


	/*
	std::size_t nVertices = cube.nVertices;
	for (int i = 0; i < nVertices; i++)
	{
		
		Vec3 v = transformP3( cube.vertices[ i ], transformMatrix );
		int x  = ( v[0] * camera.zoomFactor / (width/(float)height) ) + (width / 2);
		int y  = (-v[1] * camera.zoomFactor ) + (height / 2);
		drawPoint(x, y);
	}
	*/
}

/**
 * Maps the x coordinate to the window coordinate system
 */
inline int g3::World::mapXToWin(float x)
{
	return ( x * camera.zoomFactor / (width/(float)height)  ) + (width / 2.0f);
}

/**
 * Maps the y coordinate to the window coordinate system
 */
inline int g3::World::mapYToWin(float y)
{
	return ( -y * camera.zoomFactor ) + (height / 2.0f);
}


/**
 * Creates an RGBA color as a long.
 */
inline unsigned long g3::createRGBA(int r, int g, int b, int a)
{
	return ((r & 0xff) << 24) 
		+ ((g & 0xff) << 16) 
		+ ((b & 0xff) << 8) 
		+ (a & 0xff);
}


/**
 * Draws a line.
 */
void g3::World::drawLine(int x0, int y0, float z0, int x1, int y1, float z1, unsigned long color)
{

	int x = x0;
	int y = y0;
	int z = z0;

	int dx = std::abs(x1 - x0);
	int dy = std::abs(y1 - y0);
	float dz = std::abs(z1 - z0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;

	int err = dx - dy;

	float lenTotal = (dx*dx) + (dy*dy);
	float lenPart = 0;

	while (true)
	{
		lenPart = ((x0-x)*(x0-x)) + ((y0-y)*(y0-y));
		z0 = z0 + (dz * (lenPart/lenTotal) );
		drawPoint(x0, y0, z0, color);

		if ((x0 == x1) && (y0 == y1)) break;
		int e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 < dx) { err += dx; y0 += sy; }
	}
}

/**
 * Draws a point on the screen.
 */
void g3::World::drawPoint(int x, int y, float z, unsigned long color)
{

	if ((x >= 0) && (y >=0) && (x < width) && (y < height))
	{

		// depth test
		int targetPixel = y * width + x;
		if ( z < depthBuffer[targetPixel] )
		{
			// saves the new depth value
			depthBuffer[targetPixel] = z;

			// sets the color of the pixel
			int offset  = y * frontBuffer->get_rowstride() + x * frontBuffer->get_n_channels();
			guchar* pixel = &frontBuffer->get_pixels()[ offset ];

			pixel[0] = (color >> 24) & 0xff; // red
			pixel[1] = (color >> 16) & 0xff; // blue
			pixel[2] = (color >>  8) & 0xff; // grenn
		
			// alpha ignored
			// pixel[3] =  color & 0xff;         // alpha
		}
	}
}

/**
 * Returns a time point in nanoseconds.
 *
 * @return Time point in nanoseconds.
 */
unsigned long g3::World::clock_time()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return (ts.tv_sec * 1000000000) + ts.tv_nsec;
}

/**
 * This idle callback function is executed as often as possible, 
 * hence it is ideal for processing intensive tasks.
 */
bool g3::World::on_idle()
{
	// finish current frame
	finishFrameTime = clock_time();

	// The amount of time that the frame spent in nanoseconds.
	unsigned long timeSpentInFrame = finishFrameTime - startFrameTime;

	if (timeSpentInFrame <= targetFrameTime)
	{
		// sleep
		std::chrono::duration<float, std::nano> wait { targetFrameTime - timeSpentInFrame };
		std::this_thread::sleep_for( wait );

		// redraw only if we were too fast in the prevoius frame.
		queue_draw();
	}
	else
	{
		// Here we were too slow so we will skip the next redrawing.
		std::cout << "Frame dropping: " << timeSpentInFrame << std::endl;
	}
	
	// start new frame
	startFrameTime = clock_time();

	// Elapsed time calculation between two frames goes here
	// ...

	// Rotates the cube around the y axis in radians.
	cube.rotationX += 0.01;
	cube.rotationY += 0.01;
	cube.rotationZ += 0.01;

	return true;
}


