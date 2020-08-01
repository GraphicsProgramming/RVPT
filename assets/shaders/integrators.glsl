/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*                                                                          */
/*                             INTEGRATORS					                */
/*                   									                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

/*
	TODO:
	
	- Add rasterizer-like integrator.
	- Add Appel integrator.
	- Add Whitted integrator.
	- Add Kajiya integrator.
	- Add albedo integrator.
	- Add specific bounce integrator.
	- Add Cook integrator?
	- Add Hart integrator?
*/

/*--------------------------------------------------------------------------*/

vec3 integrator_binary

	(Ray   ray,      /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt)     /* upper bound for t */
	 
/*
	Returns (1,1,1) for primary ray intersections and (0,0,0) otherwise.
*/
	 
{

	return vec3(intersect_scene_any(ray, mint, maxt));

} /* integrator_binary */

/*--------------------------------------------------------------------------*/

vec3 integrator_depth

	(Ray   ray,      /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt)     /* upper bound for t */
	 
/*
	Returns the reciprocal distance to the intersection 
	from the primary ray's origin.
*/
	 
{
	Isect info;
	intersect_scene (ray, mint, maxt, info);
	
	/* find the distance by taking into account direction's magnitude */
	float inv_dist = 1.0 / (length(ray.direction) * info.t);
	return vec3(inv_dist);

} /* integrator_depth */

/*--------------------------------------------------------------------------*/

vec3 integrator_normal

	(Ray   ray,      /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt)     /* upper bound for t */
	 
/*
	Upon intersection returns a color constructed as:
	0.5 * normal + 0.5. If there is no intersection returns (0,0,0).
*/
	 
{
	Isect info;
	float isect = float(intersect_scene (ray, mint, maxt, info));
	return 0.5*info.normal + vec3(0.5*isect);

} /* integrator_normal */

/*--------------------------------------------------------------------------*/

vec3 integrator_ao

	(Ray   ray,      /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt,     /* upper bound for t */
	 int   nrays)    /* # of rays used to estimate the AO integral */
/*
	Computes ambient occlusion by using Monte Carlo to estimate 
	the integral:
	
	\frac{1}{\pi} \int_{H}\cos\theta V(x,\omega) d\omega
	
	where H is the hemisphere centered around the normal at 
	x, and the visibility function V is implicitly bounded by 
	(mint, maxt).
*/ 
	
{
	Isect info;
	bool isect = intersect_scene (ray, mint, maxt, info);
	if (!isect) return vec3(0);
	
	/* shoot several rays to estimate the occlusion integral */
	float acc = 0.0;
	for (int i=0; i<nrays; ++i)
	{
		Ray new_ray;
		new_ray.origin = info.pos;
		float u = rand();
		float v = rand();
		
		/* diffuse scattering, pdf cancels out \cos\theta / \pi factor */
		new_ray.direction = map_cosine_hemisphere_simple(u,v,info.normal);
		
		/* accumulate occlusion */
		acc += float(intersect_scene_any(new_ray, mint, maxt));
	}
	
	return vec3(1.0-acc/float(nrays));

} /* integrator_ao */

/*--------------------------------------------------------------------------*/

vec3 integrator_Kajiya

	(Ray   ray,      /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt,     /* upper bound for t */
	 int   nbounce)  /* max # of bounces */
	 
/*
	An integrator based on the rendering equation as described 
	in Kajiya's paper (standard path tracing):
    
    The Rendering Equation, James Kajiya, 1986
	
	under construction
*/
	 
{

	Ray temp_ray = ray;
	Isect info;
	
	vec3 col = vec3(0);
	vec3 throughput = vec3(1);
	vec3 background;
	
	for (int i=0; i<nbounce; ++i)
	{
	
        /* intersected nothing -> background */
        if (!intersect_scene (temp_ray, mint, maxt, info))
            return col + throughput*mix(vec3(1),vec3(0.2,0.3,0.7), temp_ray.direction.y);
        
        /* intersected an object -> add emission */
        col += throughput*info.mat.emissive;
        
        /* intersection data */
        vec3 pos = info.pos;
        vec3 normal = info.normal;
        vec3 dir_in = normalize(temp_ray.direction);
        vec3 dir_out;
        
        /* cos angle with the normal */
        float cos_theta = dot(dir_in, normal);
        /* the absolute value of the cosine */
        float cos_in;
        /* whether the normal needs to be flipped */
        bool flipped_normal = cos_theta > 0.0;
        /* indices of refraction (ior) on the inside */
        float eta = info.mat.ior;
        /* ray arrives from the "inside" */
        if (flipped_normal)
        {
            /* cos_theta > 0 */
            cos_in = cos_theta;
            /* flip normal */
            normal = -normal;
        }
        else /* ray arrives from the outside */
        {
            cos_in = -cos_theta; /* cos_theta <= 0 */
            /* flip ior ratio, assume outside is always air (1.0) */
            eta = 1.0/eta;
        }
        
        /* Handle different materials */
        switch (info.mat.type)
        {
        case 0: /* Lambert */
            /* offset to upper hemisphere to avoid self-intersection */
            temp_ray.origin = pos + EPSILON * normal;
            /* scatter cosine weighted */
            temp_ray.direction = mat_scatter_Lambert_cos(normal);
            throughput *= mat_eval_Lambert_cos(info.mat.base_color/PI);
            break;
            
        case 1: /* perfect mirror */
            /* offset to upper hemisphere to avoid self-intersection */
            temp_ray.origin = pos + EPSILON * normal;
            /* reflect */
            temp_ray.direction = dir_in + 2*cos_in*normal;
            throughput *= mat_eval_mirror(info.mat.base_color);    
            break;
            
        case 2: /* dielectric */
        {
            float cos_out_sqr = 1.0 - eta*eta * (1.0-cos_in*cos_in);
            if (cos_out_sqr<=0) /* total internal reflection */
            {
                /* upper hemisphere offset */
                temp_ray.origin = pos + EPSILON * normal;       
                /* reflection */
                temp_ray.direction = dir_in + 2*cos_in*normal;
            }
            else
            {
                /* refraction cosine */
                float cos_out = sqrt(max(0,cos_out_sqr));
                /* Fresnel reflectance */
                float frefl = frensel_reflectance(cos_in,cos_out,eta);
                
                /* reflection due to Fresnel */
                if (rand() < frefl)
                {
                    /* upper hemisphere offset */
                    temp_ray.origin = pos + EPSILON * normal; 
                    /* reflection */
                    temp_ray.direction = dir_in + 2*cos_in*normal;
                }
                else /* refraction */
                {
                    /* lower hemisphere offset */
                    temp_ray.origin = pos - EPSILON * normal;
                    /* refraction, cos_in = -dot(d,n) */
                    temp_ray.direction = eta*dir_in 
                                       + (eta*cos_in-cos_out)*normal;
                }
            }
            throughput *= mat_eval_dielectric(info.mat.base_color);   
            break;
        }
        default:
            return vec3(0);
        }
	
	}
	
	/* return black if it runs out of iterations */
	return vec3(0);

} /* integrator_Kajiya */
	 
/*--------------------------------------------------------------------------*/

/* integrator_Utah */

/* integrator_Appel */

/* integrator_Whitted */

/* integrator_Kajiya */

/* integrator_Hart */

