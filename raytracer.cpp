// [header]
// A very basic raytracer example.
// [/header]
// [compile]
// c++ -o raytracer -O3 -Wall raytracer.cpp
// [/compile]
// [ignore]
// Copyright (C) 2012  www.scratchaimage.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// [/ignore]
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <vector>
#include <iostream>
#include <cassert>
#include <mpi.h>

#if defined __linux__ || defined __APPLE__
// "Compiled for Linux
#else
// Windows doesn't define these values by default, Linux does
#define M_PI 3.141592653589793
#define INFINITY 1e8
#endif

template<typename T>
class Vec3
{
public:
    T x, y, z;
    Vec3() : x(T(0)), y(T(0)), z(T(0)) {}
    Vec3(T xx) : x(xx), y(xx), z(xx) {}
    Vec3(T xx, T yy, T zz) : x(xx), y(yy), z(zz) {}
    Vec3& normalize()
    {
        T nor2 = length2();
        if (nor2 > 0) {
            T invNor = 1 / sqrt(nor2);
            x *= invNor, y *= invNor, z *= invNor;
        }
        return *this;
    }
    Vec3<T> operator * (const T &f) const { return Vec3<T>(x * f, y * f, z * f); }
    Vec3<T> operator * (const Vec3<T> &v) const { return Vec3<T>(x * v.x, y * v.y, z * v.z); }
    T dot(const Vec3<T> &v) const { return x * v.x + y * v.y + z * v.z; }
    Vec3<T> operator - (const Vec3<T> &v) const { return Vec3<T>(x - v.x, y - v.y, z - v.z); }
    Vec3<T> operator + (const Vec3<T> &v) const { return Vec3<T>(x + v.x, y + v.y, z + v.z); }
    Vec3<T>& operator += (const Vec3<T> &v) { x += v.x, y += v.y, z += v.z; return *this; }
    Vec3<T>& operator *= (const Vec3<T> &v) { x *= v.x, y *= v.y, z *= v.z; return *this; }
    Vec3<T> operator - () const { return Vec3<T>(-x, -y, -z); }
    T length2() const { return x * x + y * y + z * z; }
    T length() const { return sqrt(length2()); }
    friend std::ostream & operator << (std::ostream &os, const Vec3<T> &v)
    {
        os << "[" << v.x << " " << v.y << " " << v.z << "]";
        return os;
    }
};

typedef Vec3<float> Vec3f;

class Sphere
{
public:
    Vec3f center;                           /// position of the sphere
    float radius, radius2;                  /// sphere radius and radius^2
    Vec3f surfaceColor, emissionColor;      /// surface color and emission (light)
    float transparency, reflection;         /// surface transparency and reflectivity
    Sphere(
        const Vec3f &c,
        const float &r,
        const Vec3f &sc,
        const float &refl = 0,
        const float &transp = 0,
        const Vec3f &ec = 0) :
        center(c), radius(r), radius2(r * r), surfaceColor(sc), emissionColor(ec),
        transparency(transp), reflection(refl)
    { /* empty */ }
    //[comment]
    // Compute a ray-sphere intersection using the geometric solution
    //[/comment]
    bool intersect(const Vec3f &rayorig, const Vec3f &raydir, float &t0, float &t1) const
    {
        Vec3f l = center - rayorig;
        float tca = l.dot(raydir);
        if (tca < 0) return false;
        float d2 = l.dot(l) - tca * tca;
        if (d2 > radius2) return false;
        float thc = sqrt(radius2 - d2);
        t0 = tca - thc;
        t1 = tca + thc;
        
        return true;
    }
};

//[comment]
// This variable controls the maximum recursion depth
//[/comment]
#define MAX_RAY_DEPTH 5

float mix(const float &a, const float &b, const float &mix)
{
    return b * mix + a * (1 - mix);
}

//[comment]
// This is the main trace function. It takes a ray as argument (defined by its origin
// and direction). We test if this ray intersects any of the geometry in the scene.
// If the ray intersects an object, we compute the intersection point, the normal
// at the intersection point, and shade this point using this information.
// Shading depends on the surface property (is it transparent, reflective, diffuse).
// The function returns a color for the ray. If the ray intersects an object that
// is the color of the object at the intersection point, otherwise it returns
// the background color.
//[/comment]
Vec3f trace(
    const Vec3f &rayorig,
    const Vec3f &raydir,
    const std::vector<Sphere> &spheres,
    const int &depth)
{
    //if (raydir.length() != 1) std::cerr << "Error " << raydir << std::endl;
    float tnear = INFINITY;
    const Sphere* sphere = NULL;
    // find intersection of this ray with the sphere in the scene
    for (unsigned i = 0; i < spheres.size(); ++i) {
        float t0 = INFINITY, t1 = INFINITY;
        if (spheres[i].intersect(rayorig, raydir, t0, t1)) {
            if (t0 < 0) t0 = t1;
            if (t0 < tnear) {
                tnear = t0;
                sphere = &spheres[i];
            }
        }
    }
    // if there's no intersection return black or background color
    if (!sphere) return Vec3f(2);
    Vec3f surfaceColor = 0; // color of the ray/surfaceof the object intersected by the ray
    Vec3f phit = rayorig + raydir * tnear; // point of intersection
    Vec3f nhit = phit - sphere->center; // normal at the intersection point
    nhit.normalize(); // normalize normal direction
    // If the normal and the view direction are not opposite to each other
    // reverse the normal direction. That also means we are inside the sphere so set
    // the inside bool to true. Finally reverse the sign of IdotN which we want
    // positive.
    float bias = 1e-4; // add some bias to the point from which we will be tracing
    bool inside = false;
    if (raydir.dot(nhit) > 0) nhit = -nhit, inside = true;
    if ((sphere->transparency > 0 || sphere->reflection > 0) && depth < MAX_RAY_DEPTH) {
        float facingratio = -raydir.dot(nhit);
        // change the mix value to tweak the effect
        float fresneleffect = mix(pow(1 - facingratio, 3), 1, 0.1);
        // compute reflection direction (not need to normalize because all vectors
        // are already normalized)
        Vec3f refldir = raydir - nhit * 2 * raydir.dot(nhit);
        refldir.normalize();
        Vec3f reflection = trace(phit + nhit * bias, refldir, spheres, depth + 1);
        Vec3f refraction = 0;
        // if the sphere is also transparent compute refraction ray (transmission)
        if (sphere->transparency) {
            float ior = 1.1, eta = (inside) ? ior : 1 / ior; // are we inside or outside the surface?
            float cosi = -nhit.dot(raydir);
            float k = 1 - eta * eta * (1 - cosi * cosi);
            Vec3f refrdir = raydir * eta + nhit * (eta *  cosi - sqrt(k));
            refrdir.normalize();
            refraction = trace(phit - nhit * bias, refrdir, spheres, depth + 1);
        }
        // the result is a mix of reflection and refraction (if the sphere is transparent)
        surfaceColor = (
            reflection * fresneleffect +
            refraction * (1 - fresneleffect) * sphere->transparency) * sphere->surfaceColor;
    }
    else {
        // it's a diffuse object, no need to raytrace any further
        for (unsigned i = 0; i < spheres.size(); ++i) {
            if (spheres[i].emissionColor.x > 0) {
                // this is a light
                Vec3f transmission = 1;
                Vec3f lightDirection = spheres[i].center - phit;
                lightDirection.normalize();
                for (unsigned j = 0; j < spheres.size(); ++j) {
                    if (i != j) {
                        float t0, t1;
                        if (spheres[j].intersect(phit + nhit * bias, lightDirection, t0, t1)) {
                            transmission = 0;
                            break;
                        }
                    }
                }
                surfaceColor += sphere->surfaceColor * transmission *
                std::max(float(0), nhit.dot(lightDirection)) * spheres[i].emissionColor;
            }
        }
    }
    
    return surfaceColor + sphere->emissionColor;
}

//[comment]
// In the main function, we will create the scene which is composed of 5 spheres
// and 1 light (which is also a sphere). Then, once the scene description is complete
// we render that scene, by calling the render() function.
//[/comment]
void generate_scene(std::vector<Sphere> &spheres, int nbSpheres,
										unsigned int height, unsigned int width,
										float invHeight, float invWidth,
										float aspectratio, float angle)
{
		/* Seed random generator */
		srand48(13);
		
		/* position, radius, surface color, reflectivity, transparency, emission
		 * color */
		/* Generate world sphere */
		spheres.push_back(Sphere(Vec3f( 0.0, -10004, -20), 10000, Vec3f(0.80, 0.20, 0.20), 0, 0.0));

		for (int i = 0; i < nbSpheres; ++i) {
				float x,y,z,rd,r,b,g,t;
				x = (rand()/(1.*RAND_MAX))*20.-10.;
				y = (rand()/(1.*RAND_MAX))*2.-1.;
				z = (rand()/(1.*RAND_MAX))*10.-25.;
				rd = (rand()/(1.*RAND_MAX))*0.9+0.1;
				r  = (rand()/(1.*RAND_MAX));
				g  = (rand()/(1.*RAND_MAX));
				b  = (rand()/(1.*RAND_MAX));
				t  = (rand()/(1.*RAND_MAX))*0.5;
				spheres.push_back(Sphere(Vec3f(x, y, z), rd, Vec3f(r, g, b), 1, t));
		}
		/* Add light source */
		spheres.push_back(Sphere(Vec3f(0.0, 20, -30), 3, Vec3f(0.00, 0.00, 0.00), 0, 0.0, Vec3f(3)));
}

void calculate_line(Vec3f *row, std::vector<Sphere> &spheres, unsigned y,
		unsigned width, unsigned height,
		float invWidth, float invHeight,
		float angle, float aspectratio)
{
		float yy = (1 - 2 * ((y + 0.5) * invHeight)) * angle;

		for (unsigned x = 0; x < width; ++x, ++row) {
				/* We now have a set of coordinates (x, y),  we  project onto
				 * [0, angle*aspectratio] x [0, angle] */
				float xx = (2 * ((x + 0.5) * invWidth) - 1) * angle * aspectratio;
				
				/* Get direction of ray from camera */
				Vec3f raydir(xx, yy, -1);
				raydir.normalize();

				/* Trace ray */	
				*row = trace(Vec3f(0), raydir, spheres, 0);
		}
}

int main(int argc, char **argv)
{
		/* Get provided support for threads, world size and world rank */
		int provided, size, rank;
		MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
		MPI_Comm_size(MPI_COMM_WORLD, &size);
		MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    unsigned int width = 1280, height = 1024;
    float invWidth = 1 / float(width), invHeight = 1 / float(height);
    float fov = 30, aspectratio = width / float(height);
    float angle = tan(M_PI * 0.5f * fov / 180.0f);
		int nbSpheres = 100;

		std::vector<Sphere> spheres;
		if (rank == 0) {
				generate_scene(spheres, nbSpheres, width, height, invWidth, invHeight,
											 aspectratio, angle);
		}
		
		MPI_Bcast(spheres.data(), 3*nbSpheres, MPI_FLOAT, 0, MPI_COMM_WORLD);
		
		int line;
		Vec3f *row = new Vec3f[width];
		MPI_Status status;
		if (rank == 0) {
				/* Thy bidding, master? */
    		Vec3f *image = new Vec3f[width * height];

				/* Send initial tasks */
				for (line = 0; line < size - 1 && line < height; ++line) {
						MPI_Send(&line, 1, MPI_INT, line + 1, 0, MPI_COMM_WORLD); 
				}
				for (;line < height; ++line) {
						MPI_Recv(row, 3*width, MPI_FLOAT, MPI_ANY_SOURCE, MPI_ANY_TAG,
										 MPI_COMM_WORLD, &status);

						/* Memcpy the received stuff */
						for (int x = 0; x < width; ++x)
								image[status.MPI_TAG * width + x] = row[x];

						/* Send more work */
						MPI_Send(&line, 1, MPI_INT, status.MPI_SOURCE, 0,
										 MPI_COMM_WORLD);
				}

				/* Signal work done */

				for (int slave = 1; slave < size; ++slave) {
						MPI_Recv(row, 3*width, MPI_FLOAT, MPI_ANY_SOURCE, MPI_ANY_TAG,
										 MPI_COMM_WORLD, &status);

						/* Memcpy the received stuff */
						for (int x = 0; x < width; ++x)
								image[status.MPI_TAG * width + x] = row[x];

						/* Send height to put slave to rest */
						MPI_Send(&height, 1, MPI_INT, slave, status.MPI_SOURCE, MPI_COMM_WORLD);
				}
				/* Save picture */
				std::ofstream ofs("./untitled.ppm", std::ios::out | std::ios::binary);
				ofs << "P6\n" << width << " " << height << "\n255\n";
				for (unsigned i = 0; i < width * height; ++i) {
						ofs << (unsigned char)(std::min(float(1), image[i].x) * 255) <<
									 (unsigned char)(std::min(float(1), image[i].y) * 255) <<
									 (unsigned char)(std::min(float(1), image[i].z) * 255);
				}
				ofs.close();

				/* Deallocate */
				delete [] image;

		} else {
				/* Work, work */
				do {
						MPI_Recv(&line, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD,
										 &status);
						if (line < height) {
								calculate_line(row, spheres, line, width, height, invWidth,
															 invHeight, angle, aspectratio);
								MPI_Send(row, 3*width, MPI_FLOAT, 0, line,
												 MPI_COMM_WORLD); 
						}
				} while(line < height);
		}

		delete [] row;
    return 0;
}


