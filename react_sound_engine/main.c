#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#define WIDTH 100
#define HEIGHT 20

char graph[WIDTH][HEIGHT];

void clear_display()
{
	memset(graph, ' ', WIDTH * HEIGHT);
}

void draw_graphic()
{
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++)
			putc(graph[x][y], stdout);

		putc('\n', stdout);
	}
}

void set_symbol(int x, int y, int sym)
{
	/* clamp position in 2d array */
	if (x < 0)
		x = 0;
	if (x > WIDTH-1)
		x = WIDTH-1;

	if (y < 0)
		y = 0;
	if (y > WIDTH - 1)
		y = WIDTH - 1;

	graph[x][y] = (char)sym;
}

float(__cdecl *trig_func)(float x) = sinf;

void generate_graphic(const float *p_array, int count)
{
	int y_center = HEIGHT / 2;
	for (int x = 0; x < WIDTH - 1; x++)
		graph[x][y_center] = '-';

	graph[WIDTH - 1][y_center] = '>';

	float frequency = 10.0;
	float amplitude = 0.5;
	float PI = 3.14f;
	float PI2 = PI * 2;
	for (int x = 0; x < WIDTH; x++) {
		float seg = (x / (float)WIDTH);
		float sin_value = trig_func(frequency * seg * PI2) * amplitude;
		set_symbol(seg * WIDTH, y_center - (int)(sin_value * (float)y_center), '*');
	}
	draw_graphic();
}

int main()
{
	//int max = 20;
	//int index;
	//float speed = 1.5f;
	//for (size_t i = 0; i <= max; i++) {
	//	index = (int)((float)i * speed);
	//	if (index <= max)
	//		printf("%d ", index);
	//}

	float PI = 3.14;
	//float PI2 = PI * 2;
	//float time = 100.f;
	//for (float i = 0.f; i < time; i += 0.1f) {
	//	float funit = sinf(i * PI2);
	//	printf("%f ", funit);
	//}

	clear_display();
	generate_graphic(0, 0);

	return 0;
}