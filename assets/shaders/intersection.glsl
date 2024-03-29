/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*                                                                          */
/*                             INTERSECTION					                */
/*                   									                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

/*
	TODO:
	
	- Fix triangles to use vec3 for vertices and not vec4.
	- Profile if const in and so on is faster.
	- Profile whether intersect_any vs intersect is slower/faster.
	- Add uvs for texturing.
	- Use mat4 or mat3 + vec3 for sphere data (allows ellipsoids).
	- Bring this further to instancing.
	- Add material data from intersection.
	
	- Add aabb intersection.
	- Add torus intersection.
	- Add cylinder intersection.
	- Add disk intersection.
	- Add quadrics intersection.
	- Add splines/patches intersection.
	- Write derivations for all new introduced intersections in the 
	appendix.

*/

/*--------------------------------------------------------------------------*/

struct Material_new
{
	int  type; /* 0: diffuse, 1: perfect mirror */
	vec3 base_color;
	vec3 emissive;
	float ior;
};

Material_new convert_old_material

	(Material mat)
	
{
	Material_new res;
	res.type = int(mat.data.x);
	res.base_color = mat.albedo.xyz;
	res.emissive = mat.emission.xyz;
    res.ior = mat.albedo.w;
	
	return res;
}

struct Isect

/*
	Structure containing the resulting data from an intersection.
*/

{
	float t;      /* coordinate along the ray, INF -> none */
	vec3  pos;    /* position in global coordinates */
	vec3  normal; /* normal in global coordinates */
	vec2  uv;     /* surface parametrization (for textures) */
	Material_new mat;
	
}; /* Isect */

/*--------------------------------------------------------------------------*/

bool intersect_plane_any

	(Ray   ray,  /* ray for the intersection */
	 float d,    /* offset of the plane: <o,n>, o any point on the plane */
	 vec3  n,    /* normal of the plane (not necessarily unit length) */
	 float mint, /* lower bound for t */
	 float maxt) /* upper bound for t */
	
/*
	Returns true if there is an intersection with the plane with the 
	equation: <p,n> = d. The intersection is accepted if it is in 
	(mint, maxt) along the ray.
	
	For the derivation see the appendix.
*/
	
{
	float t = (d-dot(ray.origin, n)) / dot(ray.direction,n);
	return mint < t && t < maxt;
	
} /* intersect_plane_any */

/*--------------------------------------------------------------------------*/

bool intersect_plane

	(Ray       ray,  /* ray for the intersection */
	 float     d,    /* offset of the plane: <o,n>, o: point on plane */
	 vec3      n,    /* normal of the plane (not necessarily unit length) */
	 float     mint, /* lower bound for t */
	 float     maxt, /* upper bound for t */
	 out Isect info) /* intersection data */
	
/*
	Returns true if there is an intersection with the plane with the 
	equation: <p,n> = d. The intersection is accepted if it is in 
	(mint, maxt) along the ray.
	
	Also computes the normal along the ray.
	
	For the derivation see the appendix.
*/
	
{
	float t = (d-dot(ray.origin,n)) / dot(ray.direction,n);
	
	bool isect = mint < t && t < maxt;
	
	info.t = isect ? t : INF;
	info.normal = normalize(n);
	
	return isect;
	
} /* intersect_plane */

/*--------------------------------------------------------------------------*/

bool intersect_triangle_any

	(Ray   ray,  /* ray for the intersection */
	 vec3  v0,   /* vertex 0 */
	 vec3  v1,   /* vertex 1 */
	 vec3  v2,   /* vertex 2 */
	 float mint, /* lower bound for t */
	 float maxt) /* upper bound for t */
	 
/*
	Returns true if there is an intersection with the triangle (v0,v1,v2).
	The intersection is accepted if it is in (mint, maxt) along the ray.
	Uses 3x3 matrix inversion for intersection computation.
	
	For the derivation see the appendix.
*/

{
	/* linear system matrix */
	mat3 A = mat3(ray.direction, v1-v0, v2-v0);
	
	/* formal solution A * x = b -> x = A^{-1} * b */
	vec3 sol =  inverse(A) * (ray.origin - v0);
	
	/* need to flip t, since the solution actually computes -t */
	float t = -sol.x;
	
	/* barycentric coordinates */
	float u = sol.y;
	float v = sol.z;

	return mint<t && t<maxt && 0<u && 0<v && u+v<1;
	
} /* intersect_triangle_any */

/*--------------------------------------------------------------------------*/

bool intersect_triangle

	(Ray       ray,  /* ray for the intersection */
	 vec3      v0,   /* vertex 0 */
	 vec3      v1,   /* vertex 1 */
	 vec3      v2,   /* vertex 2 */
	 float     mint, /* lower bound for t */
	 float     maxt, /* upper bound for t */
	 out Isect info) /* intersection data */

/*
	Returns true if there is an intersection with the triangle (v0,v1,v2).
	The intersection is accepted if it is in (mint, maxt) along the ray.
	Uses 3x3 matrix inversion for intersection computation.
	
	Computes also the intersection position, normal, uv coordinates.
	
	For the derivation see the appendix.
*/

{
	/* linear system matrix */
	mat3 A = mat3(ray.direction, v1-v0, v2-v0);
	
	/* formal solution A * x = b -> x = A^{-1} * b */
	vec3 sol =  inverse(A) * (ray.origin - v0);
	
	/* need to flip t, since the solution actually computes -t */
	float t = -sol.x;
	
	/* barycentric coordinates */
	float u = sol.y;
	float v = sol.z;
	
	/* is the intersection valid? */
	bool isect = mint<t && t<maxt && 0<u && 0<v && u+v<1;
	
	/* compute intersection data */
	info.t = isect ? sol.x : INF;
	info.pos = ray.origin + info.t*ray.direction;
	info.normal = normalize(cross(A[1],A[2]));
	info.uv = vec2(u,v);
	
	return isect;
	
} /* intersect_triangle */

/*--------------------------------------------------------------------------*/

bool intersect_triangle_any_fast

	(Ray   ray,  /* ray for the intersection */
	 vec3  v0,   /* vertex 0 */
	 vec3  v1,   /* vertex 1 */
	 vec3  v2,   /* vertex 2 */
	 float mint, /* lower bound for t */
	 float maxt) /* upper bound for t */
	 
/*
	Returns true if there is an intersection with the triangle (v0,v1,v2).
	The intersection is accepted if it is in (mint, maxt) along the ray.
	Uses the metric tensor for intersection computation.
	
	For the derivation see the appendix.
*/

{	
	/* edges and non-normalized normal */
	vec3 e0 = v1-v0;
	vec3 e1 = v2-v0;
	vec3 n = cross(e0,e1);
	
	/* intersect plane in which the triangle is situated */
	float t = dot(v0-ray.origin,n) / dot(ray.direction,n);
	vec3 p = ray.origin + t*ray.direction;
	
	/* intersection position relative to v0 */
	vec3 p0 = p - v0;
	
	/* transform p0 with the basis vectors */
	vec2 b = vec2(dot(p0,e0), dot(p0,e1));
	
	/* adjoint of the 2x2 metric tensor (contravariant) */
	mat2 A_adj = mat2(dot(e1,e1), -dot(e0,e1), -dot(e0,e1), dot(e0,e0));
	
	/* denominator of the inverse 2x2 metric tensor (contravariant) */
	float inv_det = 1.0/(A_adj[0][0]*A_adj[1][1]-A_adj[0][1]*A_adj[1][0]);
	
	/* barycentric coordinate */
	vec2 uv = inv_det * (A_adj * b);
	
	return mint<t && t<maxt && 0<uv.x && 0<uv.y && uv.x+uv.y<1;
	
} /* intersect_triangle_any_fast */

/*--------------------------------------------------------------------------*/

bool intersect_triangle_fast

	(Ray       ray,  /* ray for the intersection */
	 vec3      v0,   /* vertex 0 */
	 vec3      v1,   /* vertex 1 */
	 vec3      v2,   /* vertex 2 */
	 float     mint, /* lower bound for t */
	 float     maxt, /* upper bound for t */
	 out Isect info) /* intersection data */

/*
	Returns true if there is an intersection with the triangle (v0,v1,v2).
	The intersection is accepted if it is in (mint, maxt) along the ray.
	Uses the metric tensor for intersection computation.
	
	Computes also the intersection position, normal, uv coordinates.
	
	For the derivation see the appendix.
*/
	 
{
	/* edges and non-normalized normal */
	vec3 e0 = v1-v0;
	vec3 e1 = v2-v0;
	vec3 n = cross(e0,e1);
	
	/* intersect plane in which the triangle is situated */
	float t = dot(v0-ray.origin,n) / dot(ray.direction,n);
	vec3 p = ray.origin + t*ray.direction;
	
	/* intersection position relative to v0 */
	vec3 p0 = p - v0;
	
	/* transform p0 with the basis vectors */
	vec2 b = vec2(dot(p0,e0), dot(p0,e1));
	
	/* adjoint of the 2x2 metric tensor (contravariant) */
	mat2 A_adj = mat2(dot(e1,e1), -dot(e0,e1), -dot(e0,e1), dot(e0,e0));
	
	/* denominator of the inverse 2x2 metric tensor (contravariant) */
	float inv_det = 1.0/(A_adj[0][0]*A_adj[1][1]-A_adj[0][1]*A_adj[1][0]);
	
	/* barycentric coordinate */
	vec2 uv = inv_det * (A_adj * b);
	
	/* is it a valid intersection? */
	bool isect = mint<t && t<maxt && 0<uv.x && 0<uv.y && uv.x+uv.y<1;
	
	/* compute intersection data */
	info.t = isect ? t : INF;
	info.pos = ray.origin + info.t*ray.direction;
	info.normal = n;
	info.uv = uv;
	
	return isect;
	
} /* intersect_triangle_fast */

/*--------------------------------------------------------------------------*/

bool intersect_aabb

	(Ray       ray,  	   /* ray for the intersection */
	 vec3      aabb_min,   /* min vertex */
	 vec3      aabb_max,   /* max vertex */
	 float     mint,       /* minimum distance along the ray */
	 float     maxt)       /* maximum distance along the ray */

/*
	Returns true if there is an intersection with the AABB (min,max).

	For the derivation see the appendix. (Todo)
*/
{
    vec3 invdir = 1.0f / ray.direction;

    vec3 f = (aabb_max - ray.origin) * invdir;
    vec3 n = (aabb_min - ray.origin) * invdir;

    vec3 tmax = max(f, n);
    vec3 tmin = min(f, n);

    double t1 = min(tmax.x, min(tmax.y, tmax.z));
    double t0 = max(tmin.x, max(tmin.y, tmin.z));

	t0 = max(t0, mint);
	t1 = min(t1, maxt);

	return t1 >= t0;

} /* intersect_aabb */

/*--------------------------------------------------------------------------*/

bool intersect_bvh(in Ray ray, float mint, float maxt, out Isect info)
{
	uint[64] stack;
	int stack_ptr = 0;

	float closest_t = maxt;
	info.t = INF;
	info.pos = vec3(0);
	info.normal = vec3(0);

	stack[stack_ptr++] = ~0;
	uint stack_top = 0;
	while (stack_top != ~0)
	{
		BvhNode node = bvh_nodes[stack_top];
		vec3 node_min = vec3(node.bounds[0], node.bounds[2], node.bounds[4]);
		vec3 node_max = vec3(node.bounds[1], node.bounds[3], node.bounds[5]);
		if (!intersect_aabb(ray, node_min, node_max, mint, closest_t)) {
			stack_top = stack[--stack_ptr];
			continue;
		}

		uint first_child_or_primitive = node.first_child_or_primitive;
		if (node.primitive_count > 0)
		{
			// This is a leaf
			for (uint i = first_child_or_primitive, n = i + node.primitive_count; i < n; ++i)
			{
				Triangle triangle = triangles[i];
				vec3 v0 = triangle.vert0.xyz;
				vec3 v1 = triangle.vert1.xyz;
				vec3 v2 = triangle.vert2.xyz;
				Isect temp_isect;
				if (intersect_triangle_fast(ray, v0, v1, v2, mint, closest_t, temp_isect)) {
					info = temp_isect;
					Material mat = materials[int(triangle.mat_id.x)];
					info.mat = convert_old_material(mat);
					closest_t = temp_isect.t;
				}
			}
			stack_top = stack[--stack_ptr];
		}
		else
		{
			// This is an internal node: Push its children on the stack.
			// TODO: Order the children on the stack
			stack[stack_ptr++] = first_child_or_primitive + 1;
			stack_top = first_child_or_primitive;
		}
	}

	return closest_t < maxt;
}

/*--------------------------------------------------------------------------*/

bool intersect_bvh_any(in Ray ray, float mint, float maxt)
{
	uint[64] stack;
	int stack_ptr = 0;

	float closest_t = maxt;

	stack[stack_ptr++] = ~0;
	uint stack_top = 0;
	while (stack_top != ~0)
	{
		BvhNode node = bvh_nodes[stack_top];
		vec3 node_min = vec3(node.bounds[0], node.bounds[2], node.bounds[4]);
		vec3 node_max = vec3(node.bounds[1], node.bounds[3], node.bounds[5]);
		if (!intersect_aabb(ray, node_min, node_max, mint, closest_t)) {
			stack_top = stack[--stack_ptr];
			continue;
		}

		uint first_child_or_primitive = node.first_child_or_primitive;
		if (node.primitive_count > 0)
		{
			// This is a leaf
			for (uint i = first_child_or_primitive, n = i + node.primitive_count; i < n; ++i)
			{
				Triangle triangle = triangles[i];
				vec3 v0 = triangle.vert0.xyz;
				vec3 v1 = triangle.vert1.xyz;
				vec3 v2 = triangle.vert2.xyz;
				Isect temp_isect;
				if (intersect_triangle_fast(ray, v0, v1, v2, mint, closest_t, temp_isect)) {
					return true;
				}
			}
			stack_top = stack[--stack_ptr];
		}
		else
		{
			// This is an internal node: Push its children on the stack.
			// TODO: Order the children on the stack
			stack[stack_ptr++] = first_child_or_primitive + 1;
			stack_top = first_child_or_primitive;
		}
	}

	return false;
}

/*--------------------------------------------------------------------------*/

bool intersect_scene_any

	(Ray   ray,  /* ray for the intersection */
	 float mint, /* lower bound for t */
	 float maxt) /* upper bound for t */
	 
/*
	Returns true if there is an intersection with any primitive in the 
	scene in (mint, maxt). 
	
	Note: this intersection routine has an early out through an `if`, idk 
	if it does more bad than good in the sense of divergence, but 
	it allows skipping any check after the first intersection. 
*/
	 
{
	return intersect_bvh_any(ray, mint, maxt);
	
} /* intersect_scene_any */

/*--------------------------------------------------------------------------*/

bool intersect_scene

	(Ray       ray,  /* ray for the intersection */
	 float     mint, /* lower bound for t */
	 float     maxt, /* upper bound for t */
	 out Isect info) /* intersection data */
	 
{
	float closest_t = INF;
	info.t = closest_t;
	info.pos = vec3(0);
	info.normal = vec3(0);
	Isect temp_isect;

	/* Intersect BVHs and get the primitives that are possibly intersected (Triangles Only) */

	if (intersect_bvh(ray, mint, closest_t, temp_isect))
	{
		closest_t = temp_isect.t;
		info = temp_isect;
	}

	info.normal = closest_t<INF? normalize(info.normal) : vec3(0);

	info.pos = closest_t<INF? ray.origin + info.t * ray.direction : vec3(0);

	return closest_t<INF;
	
} /* intersect_scene */

/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*                                                                          */
/*                          INTERSECTION APPENDIX			                */
/*                   									                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

/*
	Ray-Sphere intersection derivation:
	
	1) Ray in parametric form:
	r(t) = r.o + t*r.d
	
	2) Unit sphere centered at (0,0,0) canonical form:
	||p||^2 = 1^2
	
	3) For an intersecting point both 1) and 2) must hold.
	Substitute:
	p = r(t) = r.o + t*t.d ->
	
	||r.o + t*r.d||^2 = 1 ->
	
	||r.d||^2 * t^2 + 2*dot(r.o,r.d) * t + ||r.o||^2 - 1 = 0 ->
	
	A = dot(r.d, r.d), B = -dot(r.o,r.d), C = dot(r.o r.o) - 1 ->
	
	4) Quadratic equation:
	A * t^2 - 2*B * t + C = 0
	
	D = B*B - A*C, D<=0 -> no intersection
	(we ignore 1 point intersections since those have measure 0 
	and are irrelevant in an implementation with finite precision)
	
	5) If D>0 there are 2 intersections, we needs the closest 
	valid one from those. An intersection is valid if t is in (mint, maxt).
	
	The INF in the implementation guarantee that when there is 
	no intersection it will be rejected. It would have been nice if 
	sqrt(D) produced a NaN when D<0, but unfortunately it is undefined.
*/

/*--------------------------------------------------------------------------*/

/*

	Ray-Plane intersection derivation:
	
	1) Ray in parametric form:
	r(t) = r.o + t*r.d
	
	2) Plane in canonical form (cannot derive uv coords from it):
	dot(p,n) = d, if a point c is known on the plane then d = dot(o,n)
	
	n is the normal to the plane (doesn't have to be unit length, but 
	is preferable returning the correct normal without normalizing).
	
	3) For an intersection both 1) and 2) must hold.
	Substitute:
	p = r(t) = r.o + t*r.d ->
	
	dot(r.o+t*r.d,n) = d -> (using dot product linearity)
	
	t*dot(r.d, n) = d - dot(r.o, n) ->
	
	t = (d-dot(r.o,n)) / dot(r.d, n)
	
	4) The intersection is considered valid if t is in (mint, maxt).
	If t is NaN or +-Inf this check naturally fails.

*/

/*--------------------------------------------------------------------------*/

/*
	Ray-Triangle intersection derivation (3x3 matrix form):
	
	1) Ray in parametric form:
	r(t) = r.o + t*r.d
	
	2) Triangle in parametric form:
	S(u,v) = v0 + u*(v1-v0) + v*(v2-v0)
	
	v0,v1,v2 are the vertices' coordinates in global coordinates.
	
	3) For an intersection both 1) and 2) must hold.
	Equate:
	S(u,v) = r(t) ->
	
	v0 + u*(v1-v0) + v*(v2-v0) = r.o + t*r.d -> (group unknowns on rhs)
	
	-t*r.d + u*(v1-v0) + v*(v2-v0) = r.o - v0
	
	3) The above are 3 equations (for x,y,z coords) with 3 unknowns.
	Rewrite it matrix form for simplicity:
	
	A * sol = b, b = r.o - v0, sol = (-t, u, v), A = [r.d | v1-v0 | v2-v0],
	the input vectors to A are organized as column vectors.
	
	4) Formal solution: sol = inv(A) * b
	t = -sol[0], u = sol[1], v = sol[2]

	5) The intersection is valid, when:
	t in (mint, maxt) <- intersection with the plane of the triangle
	0<u and 0<v and u+v<1 <- the point's barycentric coordinates are 
	within the triangle.
	
	The above tests will fail naturally if the matrix is non-invertible 
	(there is no intersection) since the solution will be made of NaNs/Infs.
	
*/

/*--------------------------------------------------------------------------*/

/*
	Ray-Triangle intersection derivation (using the metric tensor):
	
	1) Ray in parametric form:
	r(t) = r.o + t*r.d
	
	2) Triangle in parametric form:
	e0 = v1-v0, e1 = v2-v0
	S(u,v) = v0 + u*e0 + v*e1
	
	v0,v1,v2 are the vertices' coordinates in global coordinates.
	
	3) Split the problem into a plane intersection + a subsequent 
	point-inside-triangle test.
	
	The plane in which the triangle lies is defined through the equation:
	dot(p-v0,n) = 0, n = cross(e0,e1)
	
	4) Ray-plane intersection:
	Substitute:
	p = r(t) = r.o + t*r.d ->
	dot(r.o+t*r.d-v0, n) = 0 -> (using linearity)
	t = dot(v0-r.o, n) / dot(r.d, n)
	
	Check t in (mint, maxt) -> fails naturally when 
	there is no intersection due to NaNs/Infs.
	
	5) Point-inside-triangle test. The ray-plane intersection point p 
	is inside the triangle if:
	p = S(u,v) is true for some (u,v) such that: 0<u and 0<v and u+v<1
	The above is the standard condition on the barycentric coordinate.
	
	The main issue is that p = S(u,v) is an overdetermined system with 
	3 equations and 2 unknowns which can result in edge cases. The 
	system can be reduced to a system of 2 equations.
	
	6) Metric tensor system reduction. The equation:
	p = S(u,v) = v0 + u*e0 + v*e1 can be dotted on both sides 
	with e0 and e1 ->
	
	dot(e0, p-v0) = u*dot(e0,e0) + v*dot(e0,e1)
	dot(e1, p-v0) = u*dot(e1,e0) + v*dot(e1,e1)
	
	This can be rewritten in matrix form:
	
	G * sol = b, b = (dot(e0,p-v0), dot(e1,p-v0)), sol = (u,v),
	
	    [ dot(e0,e0)   dot(e0,e1) ]
	G = [                         ] 
	    [ dot(e1,e0)   dot(e1,e1) ]
	
	G is the metric tensor of the basis {e0,e1}.
	
	7) System solution.
	Inversion of a 2x2 matrix is trivial:
	    [ a   b ]             [ d  -b ]
	A = [       ] -> inv(A) = [       ] * 1/(a*d-b*c)
	    [ c   d ]             [-c   a ]
		
	The solution is given as:
	sol = inv(G) * b, inv(G) is the contravariant metric tensor.
	
	7) If the matrix is non-invertible, then either e0=0 or e1 = 0
	or e0 and e1 are parallel, in which case the determinant is 0
	and results in NaNs/Infs, and the check 0<u && 0<v && u+v<1 
	naturally fails.
	
*/

/*--------------------------------------------------------------------------*/

bool intersect_triangles(Ray ray, inout Record record)
{
    float lowest = record.distance;
    for(int i = 0; i < triangles.length(); i++){
        Triangle triangle = triangles[i];

        vec3 o = triangle.vert0.xyz;
        vec3 e0 = triangle.vert1.xyz - o;
        vec3 e1 = triangle.vert2.xyz - o;
        vec3 intersectionMat[3] = {ray.direction * -1, e0, e1};

        vec3 c01 = cross(intersectionMat[0], intersectionMat[1]);
        vec3 c12 = cross(intersectionMat[1],intersectionMat[2]);
        vec3 c20 = cross(intersectionMat[2], intersectionMat[0]);

        float det = dot(intersectionMat[0], c12);
        float inverseDet = 1.0f / det;

        vec3 inverseTransposedMat[3] = { c12*inverseDet, c20*inverseDet, c01*inverseDet };

        vec3 dir = ray.origin - o;
        vec3 tuv = vec3(
        dot(inverseTransposedMat[0], dir),
        dot(inverseTransposedMat[1], dir),
        dot(inverseTransposedMat[2], dir));

        if(0 < tuv.x && 0.0f < tuv.y && 0.0f < tuv.z && tuv.y + tuv.z < 1.0f){
            float t = tuv.x;
            if((RAY_MIN_DIST < t && (t < lowest || lowest < 0)))
            {
                lowest = t;
                record.intersection = ray.origin + ray.direction * t;
                record.distance = t;
                record.normal = vec3(triangle.vert0.w, triangle.vert1.w, triangle.vert2.w);
                record.hit = true;
                record.mat = materials[int(triangle.mat_id.x)];
                record.albedo = materials[int(triangle.mat_id.x)].albedo.xyz;
                record.emission = materials[int(triangle.mat_id).x].emission.xyz;
                //            rec.u = tuv.y * vertices[1].u + tuv.z * vertices[2].u + (1.0f - tuv.y - tuv.z) * vertices[0].u;
                //            rec.v = tuv.y * vertices[1].v + tuv.z * vertices[2].v + (1.0f - tuv.y - tuv.z) * vertices[0].v;
            }
        }
    }
    return lowest > 0;
}
