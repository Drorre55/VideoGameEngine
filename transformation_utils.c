#include "transformation_utils.h"


void rotate_x_axis(Point* vertice, float rotation_degree)
{
	vertice->y_coord = vertice->y_coord * cos(rotation_degree) - vertice->z_coord * sin(rotation_degree);
	vertice->z_coord = vertice->y_coord * sin(rotation_degree) + vertice->z_coord * cos(rotation_degree);
	return vertice;
}

void rotate_y_axis(Point* vertice, float rotation_degree)
{
	vertice->x_coord = vertice->x_coord * cos(rotation_degree) + vertice->z_coord * sin(rotation_degree);
	vertice->z_coord = vertice->x_coord * sin(rotation_degree) - vertice->z_coord * cos(rotation_degree);
	return vertice;
}

void rotate_z_axis(Point* vertice, float rotation_degree)
{
	vertice->x_coord = vertice->x_coord * cos(rotation_degree) - vertice->y_coord * sin(rotation_degree);
	vertice->y_coord = vertice->x_coord * sin(rotation_degree) + vertice->y_coord * cos(rotation_degree);
	return vertice;
}
