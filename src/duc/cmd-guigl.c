#include "config.h"

#ifdef ENABLE_OPENGL

#include "duc.h"
#include "duc-graph.h"
#include "ducrc.h"
#include "cmd.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

static int opt_bytes;
static int opt_dark;
static char *opt_database = NULL;
static char *opt_palette = NULL;
static double opt_fuzz = 0.5;
static int opt_levels = 4;
static int opt_apparent = 0;
static int opt_count = 0;
static int opt_ring_gap = 4;
static int opt_gradient = 0;

static double tooltip_x = 0;
static double tooltip_y = 0;
static enum duc_graph_palette palette = 0;
static int win_w = 600;
static int win_h = 600;
static duc_dir *dir;
static duc_graph *graph;
static double fuzz;


static void sc2fb(GLFWwindow* window, double *x, double *y)
{
	int w1, h1, w2, h2;
	glfwGetFramebufferSize(window, &w1, &h1);
	glfwGetWindowSize(window, &w2, &h2);
	if(x) *x *= (double)w1 / (double)w2;
	if(y) *y *= (double)h1 / (double)h2;
}


void cb_winsize(GLFWwindow* window, int w, int h)
{
	win_w = w;
	win_h = h;
	glViewport(0, 0, win_w, win_h);
}


static void draw(GLFWwindow *window)
{
	if(opt_levels < 1) opt_levels = 1;
	if(opt_levels > 10) opt_levels = 10;
	
	duc_size_type st = opt_count ? DUC_SIZE_TYPE_COUNT : 
	                   opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

	duc_graph_set_size(graph, win_w, win_h);
	duc_graph_set_max_level(graph, opt_levels);
	duc_graph_set_fuzz(graph, fuzz);
	duc_graph_set_palette(graph, palette);
	duc_graph_set_max_name_len(graph, 30);
	duc_graph_set_size_type(graph, st);
	duc_graph_set_exact_bytes(graph, opt_bytes);
	duc_graph_set_tooltip(graph, tooltip_x, tooltip_y);
	duc_graph_set_ring_gap(graph, opt_ring_gap);
	duc_graph_set_gradient(graph, opt_gradient);

	duc_graph_draw(graph, dir);

	glfwSwapBuffers(window);
}
	

static void cb_keyboard(GLFWwindow* window, int k, int scancode, int action, int mods)
{
	if(action != 1) return;

	if(k == '-') opt_levels--;
	if(k == '=') opt_levels++;
	if(k == '0') opt_levels = 4;
	if(k == GLFW_KEY_ESCAPE) exit(0);
	if(k == 'Q') exit(0);
	if(k == 'A') opt_apparent = !opt_apparent;
	if(k == 'C') opt_count = !opt_count;
	if(k == 'B') opt_bytes = !opt_bytes;
	if(k == 'F') fuzz = (fuzz == 0) ? opt_fuzz : 0;
	if(k == 'G') opt_gradient = !opt_gradient;
	if(k == ',') if(opt_ring_gap > 0) opt_ring_gap --;
	if(k == '.') opt_ring_gap ++;
	if(k == 'P') palette = (palette + 1) % 5;
	if(k == GLFW_KEY_BACKSPACE) {
		duc_dir *dir2 = duc_dir_openat(dir, "..");
		if(dir2) {
			duc_dir_close(dir);
			dir = dir2;
		}
	}
}


void cb_mouse_button(GLFWwindow* window, int b, int action, int mods)
{
	if(action != 1) return;

	double x, y;
	glfwGetCursorPos(window, &x, &y);
	sc2fb(window, &x, &y);

	if(b == 0) {
		duc_dir *dir2 = duc_graph_find_spot(graph, dir, x, y, NULL);
		if(dir2) {
			duc_dir_close(dir);
			dir = dir2;
		}
	}
	if(b == 3) {
		duc_dir *dir2 = duc_dir_openat(dir, "..");
		if(dir2) {
			duc_dir_close(dir);
			dir = dir2;
		}
	}
}

void cb_scroll(GLFWwindow* window, double xoffset, double yoffset)
{
	static double scroll = 0;
	scroll += yoffset;
	if(scroll < -1) { opt_levels --; scroll += 1; }
	if(scroll > +1) { opt_levels ++; scroll -= 1; }
}


void cb_mouse_motion(GLFWwindow* window, double x, double y)
{
	sc2fb(window, &x, &y);
	tooltip_x = x;
	tooltip_y = y;
}


int guigl_main(duc *duc, int argc, char *argv[])
{
	char *path = ".";
	if(argc > 0) path = argv[0];

	fuzz = opt_fuzz;

	if(opt_palette) {
		char c = tolower(opt_palette[0]);
		if(c == 's') palette = DUC_GRAPH_PALETTE_SIZE;
		if(c == 'r') palette = DUC_GRAPH_PALETTE_RAINBOW;
		if(c == 'g') palette = DUC_GRAPH_PALETTE_GREYSCALE;
		if(c == 'm') palette = DUC_GRAPH_PALETTE_MONOCHROME;
		if(c == 'c') palette = DUC_GRAPH_PALETTE_CLASSIC;
	}

	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}
	
	dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_FTL, "%s", duc_strerror(duc));
		return -1;
	}

	if(!glfwInit()) {
		duc_log(duc, DUC_LOG_FTL, "Error initializen glfw");
		return -1;
	}
	
	glfwWindowHint(GLFW_SAMPLES, 4);

	GLFWwindow* window = window = glfwCreateWindow(640, 480, "Duc", NULL, NULL);;
	if(window == NULL)
	{
		duc_log(duc, DUC_LOG_FTL, "Error creating glfw window");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	gladLoadGLES2Loader((GLADloadproc) glfwGetProcAddress);

	glfwGetFramebufferSize(window, &win_w, &win_h);

	double font_scale = 1.0;
	sc2fb(window, &font_scale, NULL);
	graph = duc_graph_new_opengl(duc, font_scale);
	
	glfwSetKeyCallback(window, cb_keyboard);
	glfwSetFramebufferSizeCallback(window, cb_winsize);
	glfwSetMouseButtonCallback(window, cb_mouse_button);
	glfwSetCursorPosCallback(window, cb_mouse_motion);
	glfwSetScrollCallback(window, cb_scroll);

	while (!glfwWindowShouldClose(window)) {
		draw(window);
		glfwWaitEvents();
	}

	glfwTerminate();

	return 0;
}

static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "show apparent instead of actual file size" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_count,     "count",      0,  DUCRC_TYPE_BOOL,   "show number of files instead of file size" },
	{ &opt_dark,      "dark",       0,  DUCRC_TYPE_BOOL,   "use dark background color" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_fuzz,      "fuzz",       0,  DUCRC_TYPE_DOUBLE, "use radius fuzz factor when drawing graph" },
	{ &opt_gradient,  "gradient",   0,  DUCRC_TYPE_BOOL,   "draw graph with color gradient" },
	{ &opt_levels,    "levels",    'l', DUCRC_TYPE_INT,    "draw up to VAL levels deep [4]" },
	{ &opt_palette,   "palette",    0,  DUCRC_TYPE_STRING, "select palette",
		"available palettes are: size, rainbow, greyscale, monochrome, classic" },
	{ &opt_ring_gap,  "ring-gap",   0,  DUCRC_TYPE_INT,    "leave a gap of VAL pixels between rings" },
	{ NULL }
};


struct cmd cmd_guigl = {
	.name = "gui",
	.descr_short = "Interactive OpenGL graphical interface",
	.usage = "[options] [PATH]",
	.main = guigl_main,
	.options = options,
	.descr_long = 
		"The 'guigl' subcommand queries the duc database and runs an interactive graphical\n"
		"utility for exploring the disk usage of the given path. If no path is given the\n"
		"current working directory is explored.\n"
		"\n"
		"The following keys can be used to navigate and alter the graph:\n"
		"\n"
		"    +           increase maximum graph depth\n"
		"    -           decrease maximum graph depth\n"
		"    0           Set default graph depth\n"
		"    a           Toggle between apparent and actual disk usage\n"
		"    b           Toggle between exact byte count and abbreviated sizes\n"
		"    c           Toggle between file size and file count\n"
		"    f           toggle graph fuzz\n"
		"    g           toggle graph gradient\n"
		"    p           toggle palettes\n"
		"    backspace   go up one directory\n"

};

#endif

/*
 * End
 */
