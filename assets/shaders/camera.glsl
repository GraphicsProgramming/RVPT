/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*                                                                          */
/*                                CAMERA					                */
/*                   									                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

Ray camera_pinhole_ray

	(float x,    /* x coordinate in [0, 1] */
	 float y)    /* y coordinate in [0, 1] */
	 
/*
	Constructs a ray passing through the film point with 
	(local) coordinates (x,y) for a pinhole/perspective camera.
*/
 
{
	float aspect = cam.params.x;
	float hfov = cam.params.y;
	float u = aspect * (2.0*x-1.0);
	float v = 2.0*y-1.0;
	float w = 1.0/tan(0.5*hfov);

    vec3 origin = cam.matrix[3].xyz;
	
    vec3 direction = (cam.matrix * vec4(u,v,w,0.0)).xyz;
    return Ray(origin, normalize(direction));

} /* camera_pinhole_ray */

/*--------------------------------------------------------------------------*/

Ray camera_ortho_ray

	(float x,    /* x coordinate in [0, 1] */
	 float y)    /* y coordinate in [0, 1] */
	 
/*
	Constructs a ray passing through the film point with 
	(local) coordinates (x,y) for an orthographic camera.
*/
 
{
	float aspect = cam.params.x;
	float u = aspect * (2.0*x-1.0);
	float v = 2.0*y-1.0;
	
    vec3 origin = (cam.matrix * vec4(u,v,0.0,1.0)).xyz;
    vec3 direction = cam.matrix[2].xyz;
    return Ray(origin, direction);

} /* camera_ortho_ray */

/*--------------------------------------------------------------------------*/

Ray camera_spherical_ray

	(float x,    /* x coordinate in [0, 1] */
	 float y)    /* y coordinate in [0, 1] */
	 
/*
	Constructs a ray passing through the film point with 
	(local) coordinates (x,y) for a spherical/environment camera.
*/
 
{
	float phi = x * 2 * PI;
	float theta = y * PI;
	
	vec3 origin = cam.matrix[3].xyz;
	vec3 local_dir = unit_spherical_to_cartesian(phi, theta).xzy;
    vec3 direction = (cam.matrix * vec4(local_dir, 0)).xyz;
    return Ray(origin, direction);

} /* camera_spherical_ray */

/*--------------------------------------------------------------------------*/