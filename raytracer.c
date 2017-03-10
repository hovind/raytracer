#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

#define N 3
#define MAX_RAY_DEPTH 0

#define SET_COLOUR(colour, r, g, b) { colour[0] = r; colour[1] = g; colour[2] = b; }

void
copy(float to[], float src[])
{
        memcpy(to, src, N*sizeof(src[0]));
}

void
print(float src[])
{
        size_t i;
        for (i = 0; i < N; ++i)
                printf("%f ", src[i]);
        printf("\n");
}

void
add(float res[], float lhs[], float rhs[])
{
	size_t i;
	for (i = 0; i < N; ++i)
		res[i] = lhs[i] + rhs[i];
}


void
sub(float res[], float lhs[], float rhs[])
{
	size_t i;
	for (i = 0; i < N; ++i)
		res[i] = lhs[i] - rhs[i];
}

float
dot(float lhs[], float rhs[])
{
	size_t i;
	float ret = 0;
	for (i = 0; i < N; ++i)
		ret += lhs[i] * rhs[i];
	return ret;
}

float
length_squared(float vec[])
{
	return dot(vec, vec);
}


float
length(float vec[])
{
	return sqrt(length_squared(vec));
}

void
scale(float vec[], float s)
{
	size_t i;
	for (i = 0; i < N; ++i)
		vec[i] *= s;
}

void
normalize(float vec[])
{
	scale(vec, 1.0 / length(vec));
}

float
mixf(float a, float b, float mix)
{
    return b * mix + a * (1.0 - mix);
}

void
mix(float res[], float lhs[], float rhs[], float mix)
{
	size_t i;
	for (i = 0; i < 3; ++i)
	        res[i] = mixf(lhs[i], rhs[i], mix);
}

float
fresnel(float hit_normal[], float ray_dir[], float a)
{
	return mixf(pow(1.0 + dot(ray_dir, hit_normal), 3), 1.0, a);
}

void
add_weighted(float res[], float lhs[], float a, float rhs[], float b)
{
	size_t i;
	for (i = 0; i < N; ++i)
		res[i] = a * lhs[i] + b * rhs[i];
}

void
set_colour(float colour[], float r, float g, float b)
{

	colour[0] = r;
	colour[1] = g;
	colour[2] = b;
}

void
construct(float res[], float origin[], float dir[], float length)
{
        float tmp[N];
        copy(tmp, dir);
        scale(tmp, length);
        add(res, origin, tmp);
}

void
construct_refraction_dir(float refraction_dir[], float ray_dir[], float hit_normal[], int inside)
{
	float cosi;
	float k;
	float eta = 1.1;

	if (!inside)
		eta = 1 / eta;

	cosi = -1.0 * dot(hit_normal, ray_dir);
	k = 1 - eta * eta * (1 - cosi * cosi);

	add_weighted(refraction_dir, ray_dir, eta, hit_normal, eta * cosi - sqrt(k));
	normalize(refraction_dir);
}

struct sphere {
	float center[N];
	float radius2;
	float surface_colour[3];
	float emission_colour[3];
	float transparency;
	float reflection;
};
	
int
intersect(float origin[], float dir[], struct sphere *sphere, float *entry, float *exit)
{
	float origin_to_center[N];
	float origin_to_t_length;
	float t_radius2;
	float surface_to_t_length;

	sub(origin_to_center, sphere->center, origin);
	origin_to_t_length = dot(origin_to_center, dir);
	
	if (origin_to_t_length < 0)
		 return 0;

	/* Pythagoras */
	t_radius2 = length_squared(origin_to_center) - origin_to_t_length * origin_to_t_length;
	if (t_radius2 > sphere->radius2)
		 return 0;

	surface_to_t_length = sqrt(sphere->radius2 - t_radius2);

	if (entry)
		*entry = origin_to_t_length - surface_to_t_length;
	if (exit)
		*exit = origin_to_t_length + surface_to_t_length;
	
	return 1;
}

/*
int
find_first_intersection(struct sphere *spheres, unsgined int nspheres, float origin[], float dir[], struct sphere *intersected_sphere, float *intersected_entry) {	
	float entry;
	float exit;
	int ret = 0;

	*intersected_entry = INFINITY;
	for (i = 0; i < nspheres; ++i) {
		if (intersect(spheres[i], origin, dir, &entry, &exit)) {
			if (entry < 0)
				entry = exit;
	
			if (entry < *intersected_entry) {
				// We have found a "new first intersection" 
				ret = 1;
				*intersected_entry = entry;
				intersected_sphere = &spheres[i];
			}
		}
	}
	return ret;
}*/

float
randomf(void)
{
	return rand() / (float) RAND_MAX;
}

float
randomf_in_range(float min, float max)
{
	return randomf() * (max - min) + min;
}

struct sphere *
generate_scene(int nspheres, unsigned int width, unsigned int height, float *width_inverse, float *height_inverse, float *fov, float *aspect_ratio, float *angle)
{
	unsigned int i;
	float radius;
	struct sphere *spheres = malloc(nspheres * sizeof(*spheres));

	*width_inverse = 1 / (float) width;
	*height_inverse = 1 / (float) height;
	*fov = 30.0;
	*aspect_ratio = width / (float) height;
	*angle = tanf(M_PI * 0.5 * *fov / 180.0);
	
	/* Generate world sphere */
	spheres[0].center[0] = 0.0;
	spheres[0].center[0] = -10004.0;
	spheres[0].center[0] = -20.0;
	spheres[0].radius2 = 10000.0*10000.0;

	set_colour(spheres[0].surface_colour, 0.8, 0.2, 0.2);
	set_colour(spheres[0].emission_colour, 0.0, 0.0, 0.0);

	spheres[0].transparency = 0.0;
	spheres[0].reflection = 0.0;

	for (i = 1; i < nspheres - 1; ++i) {
		spheres[i].center[0] = randomf_in_range(-10.0, 10.0);
		spheres[i].center[1] = randomf_in_range(-1.0, 1.0);
		spheres[i].center[2] = randomf_in_range(-25.0, -15.0);
		radius = randomf_in_range(0.9, 1.0);
		spheres[i].radius2 = radius*radius;

		set_colour(spheres[i].surface_colour, randomf(), randomf(), randomf());
		set_colour(spheres[i].emission_colour, 0.0, 0.0, 0.0);

		spheres[i].transparency = randomf_in_range(0.0, 0.5);
		spheres[i].reflection = 1.0;
	}
	/* Add light source */
	spheres[nspheres - 1].center[0] = 0.0;
	spheres[nspheres - 1].center[1] = 20.0;
	spheres[nspheres - 1].center[2] = -30.0;
	spheres[nspheres - 1].radius2 = 3.0*3.0;

	set_colour(spheres[nspheres - 1].surface_colour, 0.0, 0.0, 0.0);
	set_colour(spheres[nspheres - 1].emission_colour, 3.0, 3.0, 3.0);

	spheres[nspheres - 1].transparency = 0.0;
	spheres[nspheres - 1].reflection = 0.0;

	return spheres;
}

float c2cworld(unsigned int c, float measure_inverse)
{
	return 2.0 * (c + 0.5) * measure_inverse - 1.0;
}

float
y2yworld(unsigned int y, float height_inverse, float angle)
{
	return -1.0 * angle * c2cworld(y, height_inverse);
}

float
x2xworld(unsigned int x, float width_inverse, float angle, float aspect_ratio)
{
	return angle * aspect_ratio * c2cworld(x, width_inverse);
}

void
calculate_line(float *row, struct sphere *spheres, unsigned int nspheres, unsigned int y, unsigned int width, unsigned int height, float width_inverse, float height_inverse, float angle, float aspect_ratio)
{
	unsigned int x;
	float xworld;
	float yworld = y2yworld(y, height_inverse, angle);
	float origin[N] = {0.0, 0.0, 0.0};
	float dir[N];

	for (x = 0; x < width; ++x, row += 3) {
		xworld = x2xworld(x, width_inverse, angle, aspect_ratio);
			
		/* Get direction of ray from camera */
		dir[0] = xworld;
		dir[1] = yworld;
		dir[2] = -1.0;
		normalize(dir);

		/* Trace ray */	
		trace(row, origin, dir, spheres, nspheres, 0);
	}
}

float
min(float lhs, float rhs)
{
	if (lhs < rhs)
		return lhs;
	else
		return  rhs;
}

void
float2bytes(unsigned char bytes[], float floats[], size_t n)
{
	size_t i;
	for (i = 0; i < n; ++i)
		bytes[i] = min(1.0, floats[i]) * 255.0;
}

void
save_ppm(char file_name[], float image[], unsigned int width, unsigned int height)
{
	size_t i;
	FILE *fp = fopen(file_name, "wb");
	fprintf(fp, "P6\n%u %u\n255\n", width, height);
	for (i = 0; i < height * width; ++i) {
		unsigned char bytes[3];
		float2bytes(bytes, image + 3 * i, 3);
		fwrite(bytes, sizeof(bytes[0]), 3, fp);
	}
	fclose(fp);
}

int
main(int argc, char **argv)
{
	/* Get provided support for threads, world size and world rank */
	int provided;
	int size;
	int rank;
	MPI_Status status;

	size_t line;
	float *row;
	struct sphere *spheres;
	
	float width_inverse;
	float height_inverse;
	float fov;
	float aspect_ratio;
	float angle;

	unsigned int width = 1280;
	unsigned int height = 1024;
	unsigned int nspheres = 100;

	/* Init MPI */
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	/* Seed random generator with lucky number */
	srand(13);

	spheres = generate_scene(nspheres, width, height, &width_inverse,	&height_inverse, &fov, &aspect_ratio, &angle);
	row = malloc(3 * width * sizeof(*row));
	if (rank == 0) {
		/* Thy bidding, master? */
		unsigned int x;
		unsigned int slave;
		float *image = malloc(3 * width * height * sizeof(*image));

		/* Send initial tasks */
		for (line = 0; line < size - 1 && line < height; ++line)
			MPI_Send(&line, 1, MPI_INT, line + 1, 0, MPI_COMM_WORLD); 

		for (; line < height; ++line) {
			MPI_Recv(row, 3*width, MPI_FLOAT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			/* Memcpy the received stuff */
			memcpy(image + 3 * status.MPI_TAG * width, row, 3 * width * sizeof(*image));

			/* Send more work */
			MPI_Send(&line, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
		}

		/* Signal work done */

		for (slave = 1; slave < size; ++slave) {
			MPI_Recv(row, 3*width, MPI_FLOAT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			/* Memcpy the received stuff */
			memcpy(image + 3 * status.MPI_TAG * width, row, 3 * width * sizeof(*image));

			/* Send height to put slave to rest */
			MPI_Send(&height, 1, MPI_INT, slave, status.MPI_SOURCE, MPI_COMM_WORLD);
		}
		save_ppm("untitled.ppm", image, height, width);

		/* Deallocate */
		free(image);

	} else {
		/* Work, work */
		while (MPI_Recv(&line, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status), line < height) {
			calculate_line(row, spheres, nspheres, line, width, height, width_inverse, height_inverse, angle, aspect_ratio);
			MPI_Send(row, 3*width, MPI_FLOAT, 0, line, MPI_COMM_WORLD); 
		}
	}
	/* Deallocate */
	free(row);
	free(spheres);

	/* Deinit MPI */
	MPI_Finalize();
	return 0;
}

int
is_light(float colour[])
{
	int i;
	float sum = 0;
	for (i = 0; i < 3; ++i)
		sum += colour[i];
	return sum > 0;
}

float
max(float lhs, float rhs)
{
	if (lhs > rhs)
		return lhs;
	else
		return  rhs;
}

int
trace(float colour[], float ray_origin[], float ray_dir[], struct sphere *spheres, unsigned int nspheres, int depth)
{
	unsigned int i;
	float hit_point[N];
	float hit_normal[N];


	int inside = 0; 
	struct sphere *sphere = NULL;
	float distance = INFINITY;
	float bias = 1e-4;

	set_colour(colour, 0.0, 0.0, 0.0);

	for (i = 0; i < nspheres; ++i) {
		float entry;
		float exit;
		if (intersect(ray_origin, ray_dir, &spheres[i], &entry, &exit)) {
			if (entry < 0)
				entry = exit;

			if (entry < distance) {
				sphere = spheres + i;
				distance = entry;
			}
		}
	}
	if (!sphere)
		return 0;

	/* Construct hit point and hit normal */
	construct(hit_point, ray_origin, ray_dir, distance);
	sub(hit_normal, hit_point, sphere->center);
	normalize(hit_normal);

	// if the object material is glass, split the ray into a reflection
	// and a refraction ray.
	if (dot(ray_dir, hit_normal) > 0) {
		scale(hit_normal, -1.0);
		inside = 1;
	}
	if ((sphere->transparency > 0 || sphere->reflection > 0) && depth < MAX_RAY_DEPTH) {
		printf("Depth: %u\n", depth);
		/* Compute reflection */
		float refraction_colour[3];
		float reflection_colour[3];

		float reflection_origin[N];
		float reflection_dir[N];

		float fresnel_effect;

		construct(reflection_origin, hit_point, hit_normal, bias);
		construct(reflection_dir, ray_dir, hit_normal, -2.0 * dot(ray_dir, hit_normal));

		trace(reflection_colour, reflection_origin, reflection_dir, spheres, nspheres, depth + 1);

		/* Compute refraction */
		if (sphere->transparency) {
			/* Refraction origin is the hit point of ray */
			float refraction_origin[N];
			float refraction_dir[N];

			construct(refraction_origin, hit_point, hit_normal, -1.0 * bias);
			construct_refraction_dir(refraction_dir, ray_dir, hit_normal, inside);
			trace(refraction_colour, refraction_origin, refraction_dir, spheres, nspheres, depth + 1);
			scale(refraction_colour, sphere->transparency);
		}

		/* Calculate fresnel effect */
		fresnel_effect = fresnel(hit_normal, ray_dir, 0.1);
		mix(colour, reflection_colour, refraction_colour, fresnel_effect);
	} else {
		// object is a diffuse opaque object
		// compute illumination

		/* Find lights */
		for (i = 0; i < nspheres; ++i) {
			if (is_light(spheres[i].emission_colour)) {
				unsigned int j;
				float transmission_factor;
				float light_origin[N];
				float light_dir[N];
				
				construct(light_origin, hit_point, hit_normal, bias);
				sub(light_dir, spheres[i].center, hit_point);
				normalize(light_dir);
				printf("light_dir: ");
				print(light_dir);
				printf("\n");

				transmission_factor = dot(hit_normal, light_dir);
				if (transmission_factor > 0 && ray_reaches(light_origin, light_dir, i, spheres, nspheres)) {
					
					print(colour);
					printf(" + ");
					print(spheres[i].emission_colour);
					printf(" * %f = ", transmission_factor);
					construct(colour, colour, spheres[i].emission_colour, transmission_factor);
					print(colour);
					printf("\n");
				}
			}
		} 
	}
}

int
ray_reaches(float origin[], float dir[], unsigned int i, struct sphere *spheres, unsigned int nspheres)
{
	unsigned int j;
	for (j = 0; j < nspheres; ++j)
		if (i != j && intersect(origin, dir, spheres + j, NULL, NULL)) {
			printf("Interrupted %u %u!\n", i, j);
			return 0;
		}
	return 1;
}
