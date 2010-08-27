#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define pi 3.141592653589793
#define solar_mass (4 * pi * pi)
#define days_per_year 365.24

typedef double v2df __attribute__ (( vector_size(2*sizeof(double)) ));

double __inline lower(const v2df& v) {
    return ((double*)&v)[0];
}

double __inline upper(const v2df& v) {
    return ((double*)&v)[1];
}

struct planet {
    v2df xy;
    v2df z0; // z and zero
    v2df vxvy;
    v2df vz00;	// vz and zero
    v2df massmass; // the mass in both components
};

void advance(int nbodies, struct planet * bodies, const v2df& dtdt)
{
  int i, j;
  for (i = 0; i < nbodies; i++) {
    struct planet * b = &(bodies[i]);
    for (j = i + 1; j < nbodies; j++) {
      struct planet * b2 = &(bodies[j]);
      v2df dxdy = b->xy - b2->xy;
      v2df dz00 = b->z0 - b2->z0;
      v2df tsquare = __builtin_ia32_haddpd(dxdy*dxdy,dz00*dz00);    // dx*dx+dy*dy | dz*dz
      v2df distance2 = __builtin_ia32_haddpd(tsquare,tsquare);      // dx*dx+dy*dy+dz*dz | dx*dx+dy*dy+dz*dz
      v2df magmag = dtdt / (__builtin_ia32_sqrtpd(distance2)*distance2);
      dxdy *= magmag;
      dz00 *= magmag;
      b->vxvy -= dxdy * b2->massmass;
      b->vz00 -= dz00 * b2->massmass;
      b2->vxvy += dxdy * b->massmass;
      b2->vz00 += dz00 * b->massmass;
    }
  }
  for (i = 0; i < nbodies; i++) {
    bodies[i].xy += dtdt * bodies[i].vxvy;
    bodies[i].z0 += dtdt * bodies[i].vz00;
  }
}

double energy(int nbodies, struct planet * bodies)
{
  v2df e = {0.0, 0.0};
  v2df half = {0.5, 0.5};
  int i, j;

  for (i = 0; i < nbodies; i++) {
    struct planet * b = &(bodies[i]);
    v2df sq = b->massmass * __builtin_ia32_haddpd(b->vxvy*b->vxvy, b->vz00*b->vz00);  // b->mass*(vx*vx + vy*vy) | b->mass*(vz*vz + 0*0)
    sq = __builtin_ia32_haddpd(sq,sq);
    e += half * sq;
    for (j = i + 1; j < nbodies; j++) {
      struct planet * b2 = &(bodies[j]);
      v2df dxdy = b->xy - b2->xy;
      v2df dz00 = b->z0 - b2->z0;
      v2df distance = __builtin_ia32_haddpd(dxdy*dxdy, dz00*dz00);  // b->mass*(vx*vx + vy*vy) | b->mass*(vz*vz + 0*0)
      distance = __builtin_ia32_sqrtpd(__builtin_ia32_haddpd(distance,distance));
      e -= (b->massmass * b2->massmass) / distance;
    }
  }
  return lower(e);
}

void offset_momentum(int nbodies, struct planet * bodies)
{
  v2df pxpy = {0.0, 0.0};
  v2df pz00 = {0.0, 0.0};
  int i;
  for (i = 0; i < nbodies; i++) {
    pxpy += bodies[i].vxvy * bodies[i].massmass;
    pz00 += bodies[i].vz00 * bodies[i].massmass;
  }
  v2df solar_mass_inv = { 1.0 / solar_mass, 1.0 / solar_mass};
  bodies[0].vxvy = - pxpy * solar_mass_inv;
  bodies[0].vz00 = - pz00 * solar_mass_inv;
}

#define NBODIES 5
struct planet bodies[NBODIES] = {
  {                               // sun
      {0, 0}, {0,0}, {0, 0}, {0,0}, {solar_mass,solar_mass}
  },
  {                               // jupiter
      {4.84143144246472090e+00,
    -1.16032004402742839e+00},
    {-1.03622044471123109e-01, 0},
    {1.66007664274403694e-03 * days_per_year,
    7.69901118419740425e-03 * days_per_year},
    {-6.90460016972063023e-05 * days_per_year,0},
    {9.54791938424326609e-04 * solar_mass,9.54791938424326609e-04 * solar_mass}
  },
  {                               // saturn
      {8.34336671824457987e+00,
    4.12479856412430479e+00},
    {-4.03523417114321381e-01, 0},
    {-2.76742510726862411e-03 * days_per_year,
    4.99852801234917238e-03 * days_per_year},
    {2.30417297573763929e-05 * days_per_year,0},
    {2.85885980666130812e-04 * solar_mass,2.85885980666130812e-04 * solar_mass}
  },
  {                               // uranus
      {1.28943695621391310e+01,
    -1.51111514016986312e+01},
    {-2.23307578892655734e-01,0},
    {2.96460137564761618e-03 * days_per_year,
    2.37847173959480950e-03 * days_per_year},
    {-2.96589568540237556e-05 * days_per_year,0},
    {4.36624404335156298e-05 * solar_mass,4.36624404335156298e-05 * solar_mass}
  },
  {                               // neptune
      {1.53796971148509165e+01,
    -2.59193146099879641e+01},
    {1.79258772950371181e-01,0},
    {2.68067772490389322e-03 * days_per_year,
    1.62824170038242295e-03 * days_per_year},
    {-9.51592254519715870e-05 * days_per_year,0},
    {5.15138902046611451e-05 * solar_mass,5.15138902046611451e-05 * solar_mass}
  }
};

int main(int argc, char ** argv)
{
  int n = atoi(argv[1]);
  int i;

  offset_momentum(NBODIES, bodies);
  printf ("%.9f\n", energy(NBODIES, bodies));
  v2df dtdt = {0.01, 0.01};
  for (i = 1; i <= n; i++)
    advance(NBODIES, bodies, dtdt);
  printf ("%.9f\n", energy(NBODIES, bodies));
  return 0;
}

