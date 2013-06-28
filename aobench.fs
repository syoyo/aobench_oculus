//
// Fragment shader version of aobench
//
// Based on
// http://glsl.heroku.com/e#3159.0
//

#define N		(6)	// Increase N improves render quality.

uniform vec2 resolution;
uniform float time;
uniform float eye_offset;

struct Ray
{
    vec3 org;
    vec3 dir;
};
struct Sphere
{
    vec3 center;
    float radius;
};
struct Plane
{
    vec3 p;
    vec3 n;
};

struct Intersection
{
    float t;
    vec3 p;     // hit point
    vec3 n;     // normal
    int hit;
    int hitID;
};

void shpere_intersect(Sphere s,  Ray ray, inout Intersection isect)
{
    // rs = ray.org - sphere.center
    vec3 rs = ray.org - s.center;
    float B = dot(rs, ray.dir);
    float C = dot(rs, rs) - (s.radius * s.radius);
    float D = B * B - C;

    if (D > 0.0)
    {
        float t = -B - sqrt(D);
        if ( (t > 0.0) && (t < isect.t) )
        {
            isect.t = t;
            isect.hit = 1;
            isect.hitID = 1; // 1 = sphere

            // calculate normal.
            vec3 p = vec3(ray.org.x + ray.dir.x * t,
                          ray.org.y + ray.dir.y * t,
                          ray.org.z + ray.dir.z * t);
            vec3 n = p - s.center;
            n = normalize(n);
            isect.n = n;
            isect.p = p;
        }
    }
}

void plane_intersect(Plane pl, Ray ray, inout Intersection isect)
{
    // d = -(p . n)
    // t = -(ray.org . n + d) / (ray.dir . n)
    float d = -dot(pl.p, pl.n);
    float v = dot(ray.dir, pl.n);

    if (abs(v) < 1.0e-6)
        return; // the plane is parallel to the ray.

    float t = -(dot(ray.org, pl.n) + d) / v;

    if ( (t > 0.0) && (t < isect.t) )
    {
        isect.hit = 1;
        isect.hitID = 2; // 2 = plane
        isect.t   = t;
        isect.n   = pl.n;

        vec3 p = vec3(ray.org.x + t * ray.dir.x,
                      ray.org.y + t * ray.dir.y,
                      ray.org.z + t * ray.dir.z);
        isect.p = p;
    }
}

Sphere sphere[3];
Plane plane;
void Intersect(Ray r, inout Intersection i)
{
    for (int c = 0; c < 3; c++)
    {
        shpere_intersect(sphere[c], r, i);
    }
    plane_intersect(plane, r, i);
}

void orthoBasis(out vec3 basis[3], vec3 n)
{
    basis[2] = vec3(n.x, n.y, n.z);
    basis[1] = vec3(0.0, 0.0, 0.0);

    if ((n.x < 0.6) && (n.x > -0.6))
        basis[1].x = 1.0;
    else if ((n.y < 0.6) && (n.y > -0.6))
        basis[1].y = 1.0;
    else if ((n.z < 0.6) && (n.z > -0.6))
        basis[1].z = 1.0;
    else
        basis[1].x = 1.0;


    basis[0] = cross(basis[1], basis[2]);
    basis[0] = normalize(basis[0]);

    basis[1] = cross(basis[2], basis[0]);
    basis[1] = normalize(basis[1]);

}

//int seed = 0;
float random(float seed)
{
    float seed2 = (mod((seed)*1364.0+626.0, 509.0));
    return seed2/509.0;
}

vec3 computeAO(inout Intersection isect)
{
    int i, j;
    int ntheta = N;//irritating, guess I cant use a variable in a loop in webGL? Maybe I heard this already. -gtoledo
    int nphi   = N;
    float eps  = 0.0001;

    // Slightly move ray org towards ray dir to avoid numerical probrem.
    vec3 p = vec3(isect.p.x + eps * isect.n.x,
                  isect.p.y + eps * isect.n.y,
                  isect.p.z + eps * isect.n.z);

    // Calculate orthogonal basis.
    vec3 basis[3];
    orthoBasis(basis, isect.n);

    float occlusion = 0.0;

    for (int j = 0; j < N; j++)//woulda used ntheta.-gtoledo
    {
        for (int i = 0; i < N; i++)//woulda used nphi.-gtoledo
        {
            float s = isect.p.x += isect.p.y * 57. + isect.p.z * 21.;
            s = sin(cos(s) * s)*.5+.5;
            // Pick a random ray direction with importance sampling.
            // p = cos(theta) / 3.141592
            float r = random(s*0.7);
            float phi = 2.0 * 3.141592 * random(s*3000.);

            vec3 ref;
            ref.x = cos(phi) * sqrt(1.0 - r);
            ref.y = sin(phi) * sqrt(1.0 - r);
            ref.z = sqrt(r);

            // local -> global
            vec3 rray;
            rray.x = ref.x * basis[0].x + ref.y * basis[1].x + ref.z * basis[2].x;
            rray.y = ref.x * basis[0].y + ref.y * basis[1].y + ref.z * basis[2].y;
            rray.z = ref.x * basis[0].z + ref.y * basis[1].z + ref.z * basis[2].z;

            vec3 raydir = vec3(rray.x, rray.y, rray.z);

            Ray ray;
            ray.org = p;
            ray.dir = raydir;

            Intersection occIsect;
            occIsect.hit = 0;
            occIsect.t = 1.0e+30;
            occIsect.n = occIsect.p = vec3(0, 0, 0);
            Intersect(ray, occIsect);
            if (occIsect.hit != 0)
                occlusion += 1.0;
        }
    }

    // [0.0, 1.0]
    occlusion = (float(ntheta * nphi) - occlusion) / float(ntheta * nphi);

    //occlusion = (float(6 * 6) - occlusion) / float(6 * 6);
    return vec3(occlusion, occlusion, occlusion);
}

void main() {
   
    vec3  org=vec3(eye_offset, 1.0, 60.0);
    float scale = 1.0;
    vec2 pixel = -1.0 + 2.0 * vec2(gl_FragCoord.x, gl_FragCoord.y) / resolution;
    pixel *= scale;

    float tt = time;
    // compute ray origin and direction
    float asp = 1.0;
    float fovscale = 4.0;
    vec3 dir = normalize(vec3(asp*pixel.x, pixel.y, -fovscale));

    sphere[0].center = vec3(-1.0+(sin(tt*1.)), 1., -4.5+(cos(tt*1.)));
    sphere[0].radius = 1.5;
    sphere[1].center = vec3(1.5+(cos(tt*1.)), .5, -3.5+(sin(tt*1.)));
    sphere[1].radius = 1.;
    sphere[2].center = vec3(.5+(cos(tt*2.2)), 0.0, -3.2+(sin(tt*2.3)));
    sphere[2].radius = 0.5;
    plane.p = vec3(0,-0.5, 0);
    plane.n = vec3(0, 1.0, 0.0);
    
    Intersection i;
    i.hit = 0;
    i.hitID = 0;
    i.t = 1.0e+30;
    i.n = i.p = vec3(0, 0, 0);
        
    Ray r;
    r.org = org;
    r.dir = normalize(dir);
    //float seed = (mod(dir.x * dir.y * 4525434.0, 65536.0));
    
    vec4 col = vec4(0,0,0,0);
    Intersect(r, i);
    if (i.hit != 0)
    {
        vec3 hitP = r.org + i.t * r.dir;
        vec3 m = vec3(1,1,1);
        if (i.hitID == 2) { // hit plane
	   float s = 1.0;
           float checker = mod(floor(s*(hitP.z)*0.2+time), 2.0);
           float intensity = i.t * 0.005;
	   // flip
           intensity = min(1.0, max(0.0, 1.0 - intensity));
           m = (checker < 1.0) ? vec3(0.5,0.5,0.5) : vec3(1,1,1);
           m = intensity * m;
        }
        col.rgb = m * computeAO(i);
    }

    gl_FragColor = col;
}
