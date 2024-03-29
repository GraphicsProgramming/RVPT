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
	- Add specific bounce integrator.
	- Add Cook integrator?
    - Add heat map LUT for Hart.
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

vec3 integrator_color

	(Ray   ray,      /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt)     /* upper bound for t */
	 
/*
	Returns the base color of the intersected surface, otherwise (0,0,0).
*/
	 
{
    Isect info;
    if (!intersect_scene(ray, mint, maxt, info))
        return vec3(0);
    else
        return info.mat.base_color;

} /* integrator_color */

/*--------------------------------------------------------------------------*/

vec3 integrator_depth

	(Ray   ray,      /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt)     /* upper bound for t */
	 
/*
	Returns the reciprocal distance to the intersection, measured
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

vec3 integrator_Utah

	(Ray   ray,      /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt)     /* upper bound for t */
     
/*
    A rasterizer-like integrator, ignores shadows. 
    Based on the "Utah model":
    
    The Rendering Equation, James Kajiya, 1986    
*/    
    
{
    vec3 light_intensity = vec3(1.0);
    vec3 light_dir = normalize(vec3(0.5,1,0.3));
    vec3 ambient = vec3(0.1);
    
	Isect info;
	
	vec3 col = ambient;
    vec3 white = vec3(1);
    vec3 blue = vec3(0.2,0.3,0.7);

    /* intersected nothing -> background */
    if (!intersect_scene (ray, mint, maxt, info))
        return mix(white, blue, ray.direction.y);
    
     
    /* intersected an object -> add emission */
    col += info.mat.emissive;
        
    /* intersection data */
    vec3 normal = info.normal;
    
    /* choose the visible normal */
    normal = dot(ray.direction, normal) < 0.0 ? normal : -normal;

    /* Lambert's law, directional light */
    float cos_light = max(0,dot(light_dir, normal));    
    return col+info.mat.base_color * light_intensity * cos_light;
           
} /* integrator_Utah */

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
    
    For further details see: https://en.wikipedia.org/wiki/Ambient_occlusion
*/ 
	
{
	Isect info;
	bool isect = intersect_scene (ray, mint, maxt, info);
	if (!isect) return vec3(0);
	
    /* intersection data */
    vec3 normal = info.normal;
    
    /* choose the visible normal */
    normal = dot(ray.direction, normal) < 0.0 ? normal : -normal;   
    
	/* shoot several rays to estimate the occlusion integral */
	float acc = 0.0;
	for (int i=0; i<nrays; ++i)
	{
		Ray new_ray;
		new_ray.origin = info.pos + EPSILON * normal;
		float u = rand();
		float v = rand();
		
		/* diffuse scattering, pdf cancels out \cos\theta / \pi factor */
		new_ray.direction = map_cosine_hemisphere_simple(u,v,normal);
		
		/* accumulate occlusion */
		acc += float(intersect_scene_any(new_ray, mint, maxt));
	}
	
	return vec3(1.0-acc/float(nrays));

} /* integrator_ao */

/*--------------------------------------------------------------------------*/

vec3 integrator_Appel

	(Ray   ray,      /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt)     /* upper bound for t */
    
/*
    A simplistic integrator considering only a directional 
    light source and diffuse materials. Based on:
    
    Some techniques for shading machine renderings of solids, 
    Arthur Appel, 1968
*/  
  
    
{
    vec3 light_intensity = vec3(1.0);
    vec3 light_dir = normalize(vec3(0.5,1,0.3));
    
    Isect info;
    if (!intersect_scene (ray, mint, maxt, info))
        return vec3(1);
    else
    {
        vec3 dir_in = normalize(ray.direction);
        float cos_view = dot(dir_in, info.normal);
        
        /* the "correct" normal is the one that we see */
        vec3 normal = cos_view > 0.0 ? -info.normal : info.normal;
        
        Ray shadow_ray;
        /* offset to avoid self-intersection */
        shadow_ray.origin = info.pos+EPSILON*normal;
        shadow_ray.direction = light_dir;
        
        /* check visibility to light */
        if (intersect_scene_any(shadow_ray, 0, INF))
            return vec3(0);
        else
        {
            /* Lambert's law, directional light */
            float cos_light = max(0,dot(light_dir, normal));
            return light_intensity * cos_light;
        }
    }
    
} /* integrator_Appel */

/*--------------------------------------------------------------------------*/

vec3 integrator_Whitted

	(Ray   primary_ray, /* primary ray */
	 float mint,     /* lower bound for t */
	 float maxt,     /* upper bound for t */
	 int   nbounce)  /* max # of bounces */
  
/*
    An integrator that supports the Lambert brdf, 
    as well as deterministic reflection and refraction.
    Based on:
    
    An Improved Illumination Model for Shaded Display, 
    Turner Whitted, 1979
    
    Remark:
    The integrator is modified compared to the original 
    paper in order to avoid the exponential explosion of 
    rays. At each bounce, whether reflection/refraction 
    should be chosen is random.
*/
  
{
    vec3 light_intensity = vec3(1.0);
    vec3 light_dir = normalize(vec3(0.5,1,0.3));
    vec3 ambient = vec3(0.1);
    
	Ray ray = primary_ray;
	Isect info;
	
	vec3 col = ambient;
	vec3 throughput = vec3(1);
    vec3 white = vec3(1);
    vec3 blue = vec3(0.2,0.3,0.7);
	
	for (int i=0; i<nbounce; ++i)
	{
	
        /* intersected nothing -> background */
        if (!intersect_scene (ray, mint, maxt, info))
            return col + throughput*mix(white, blue, ray.direction.y);
        
        /* intersected an object -> add emission */
        col += throughput*info.mat.emissive;
        
        /* intersection data */
        vec3 pos = info.pos;
        vec3 normal = info.normal;
        vec3 dir_in = normalize(ray.direction);
        vec3 pos_out;
        vec3 dir_out;
        
        /* cos angle with the normal */
        float cos_view = dot(dir_in, normal);
        /* the absolute value of the cosine */
        float cos_in;
        /* whether the normal needs to be flipped */
        bool flipped_normal = cos_view > 0.0;
        /* indices of refraction (ior) on the inside */
        float eta = info.mat.ior;
        /* ray arrives from the "inside" */
        if (flipped_normal)
        {
            /* cos_theta > 0 */
            cos_in = cos_view;
            /* flip normal */
            normal = -normal;
        }
        else /* ray arrives from the outside */
        {
            cos_in = -cos_view; /* cos_theta <= 0 */
            /* flip ior ratio, assume outside is always air (1.0) */
            eta = 1.0/eta;
        }
        
        /* Handle different materials */
        switch (info.mat.type)
        {
        case 0: /* direct Lambert */   
            /* check light visibility */
            Ray shadow_ray;
            /* offset to avoid self-intersection */
            shadow_ray.origin = pos + EPSILON * normal;
            shadow_ray.direction = light_dir;
            
            /* check visibility to light */
            if (intersect_scene_any(shadow_ray, 0, INF))
                return col;
            else
            {
                /* Lambert's law, directional light */
                float cos_light = max(0,dot(light_dir, normal));    
                return col+throughput * info.mat.base_color * 
                       light_intensity * cos_light;
            }
            break;
            
        case 1: /* perfect mirror */
            /* offset to upper hemisphere to avoid self-intersection */
            pos_out = pos + EPSILON * normal;
            /* reflect */
            dir_out = dir_in + (cos_in+cos_in)*normal;
            throughput *= mat_eval_mirror(info.mat.base_color);    
            break;
            
        case 2: /* dielectric */
        {
            float cos_out_sqr = 1.0 - eta*eta * (1.0-cos_in*cos_in);
			float cos_out, f_refl;
			
			bool refl = (cos_out_sqr<=0);
			if(!refl) {
				/* refraction cosine */
				cos_out = sqrt(max(0,cos_out_sqr));
				/* Fresnel reflectance */
				f_refl = frensel_reflectance(cos_in,cos_out,eta);
            
				/* total internal reflection or Fresnel reflectance */
				refl = (rand() < f_refl);
			}
            
            if (refl)
            {
                /* upper hemisphere offset */
                pos_out = pos + EPSILON * normal;       
                /* reflection */
                dir_out = dir_in + (cos_in + cos_in)*normal;
            }
            else
            {
                /* lower hemisphere offset */
                pos_out = pos - EPSILON * normal;
                /* refraction, cos_in = -dot(d,n) */
                dir_out = eta*dir_in + (eta*cos_in-cos_out)*normal;
            }
            throughput *= mat_eval_dielectric(info.mat.base_color);   
            break;
        }
        default:
            return vec3(0);
        }
        
        /* prepare next ray */
        ray = Ray(pos_out, dir_out);
	}
	
	/* return black if it runs out of iterations */
	return vec3(0);
  
} /* integrator_Whitted */

/*--------------------------------------------------------------------------*/

vec3 integrator_Cook

	(Ray   primary_ray, /* primary ray */
	 float mint,        /* lower bound for t */
	 float maxt,        /* upper bound for t */
	 int   nbounce)     /* max # of bounces */
     
/*
    An integrator that does not allow for indirect light 
    due to non-specular bounces. Based on:
    
    Distributed Ray Tracing, Robert Cook, 1984
*/
     
{
	Ray ray = primary_ray;
	Isect info;
	
	vec3 col = vec3(0);
	vec3 throughput = vec3(1);
    vec3 white = vec3(1);
    vec3 blue = vec3(0.2,0.3,0.7);
	vec3 background;
	
	for (int i=0; i<nbounce; ++i)
	{
	
        /* intersected nothing -> background */
        if (!intersect_scene (ray, mint, maxt, info))
            return col + throughput*mix(white, blue, ray.direction.y);
        
        /* intersected an object -> add emission */
        col += throughput*info.mat.emissive;
        
        /* intersection data */
        vec3 pos = info.pos;
        vec3 normal = info.normal;
        vec3 dir_in = normalize(ray.direction);
        vec3 pos_out;
        vec3 dir_out;
        
        /* cos angle with the normal */
        float cos_view = dot(dir_in, normal);
        /* the absolute value of the cosine */
        float cos_in;
        /* whether the normal needs to be flipped */
        bool flipped_normal = cos_view > 0.0;
        /* indices of refraction (ior) on the inside */
        float eta = info.mat.ior;
        /* ray arrives from the "inside" */
        if (flipped_normal)
        {
            /* cos_theta > 0 */
            cos_in = cos_view;
            /* flip normal */
            normal = -normal;
        }
        else /* ray arrives from the outside */
        {
            cos_in = -cos_view; /* cos_theta <= 0 */
            /* flip ior ratio, assume outside is always air (1.0) */
            eta = 1.0/eta;
        }
        
        /* Handle different materials */
        switch (info.mat.type)
        {
        case 0: /* Lambert */
            /* offset to upper hemisphere to avoid self-intersection */
            pos_out = pos + EPSILON * normal;
            /* scatter cosine weighted */
            dir_out = mat_scatter_Lambert_cos(normal);
            throughput *= mat_eval_Lambert_cos(info.mat.base_color*INV_PI);
            
            /* bounce once more */
            ray = Ray(pos_out, dir_out);
            /* intersected nothing -> background */
            if (!intersect_scene (ray, mint, maxt, info))
                return col + throughput*mix(white, blue, ray.direction.y);
            else /* intersected an object -> add emission */
                return col + throughput*info.mat.emissive;
                
            break;
            
        case 1: /* perfect mirror */
            /* offset to upper hemisphere to avoid self-intersection */
            pos_out = pos + EPSILON * normal;
            /* reflect */
            dir_out = dir_in + (cos_in+cos_in)*normal;
            throughput *= mat_eval_mirror(info.mat.base_color);    
            break;
            
        case 2: /* dielectric */
        {
            float cos_out_sqr = 1.0 - eta*eta * (1.0-cos_in*cos_in);
			float cos_out, f_refl;
			
			bool refl = (cos_out_sqr<=0);
			if(!refl) {
				/* refraction cosine */
				cos_out = sqrt(max(0,cos_out_sqr));
				/* Fresnel reflectance */
				f_refl = frensel_reflectance(cos_in,cos_out,eta);
            
				/* total internal reflection or Fresnel reflectance */
				refl = (rand() < f_refl);
			}
            
            if (refl)
            {
                /* upper hemisphere offset */
                pos_out = pos + EPSILON * normal;       
                /* reflection */
                dir_out = dir_in + (cos_in + cos_in)*normal;
            }
            else
            {
                /* lower hemisphere offset */
                pos_out = pos - EPSILON * normal;
                /* refraction, cos_in = -dot(d,n) */
                dir_out = eta*dir_in + (eta*cos_in-cos_out)*normal;
            }
            throughput *= mat_eval_dielectric(info.mat.base_color);   
            break;
        }
        default:
            return vec3(0);
        }
        
        /* prepare next ray */
        ray = Ray(pos_out, dir_out);
	}
	
	/* return black if it runs out of iterations */
	return vec3(0); 
 
} /* integrator_Cook */

/*--------------------------------------------------------------------------*/

vec3 integrator_Kajiya

	(Ray   primary_ray, /* primary ray */
	 float mint,        /* lower bound for t */
	 float maxt,        /* upper bound for t */
	 int   nbounce)     /* max # of bounces */
	 
/*
	An integrator based on the rendering equation as described 
	in Kajiya's paper (standard path tracing):
    
    The Rendering Equation, James Kajiya, 1986
	
	under construction
*/
	 
{

	Ray ray = primary_ray;
	Isect info;
	
	vec3 col = vec3(0);
	vec3 throughput = vec3(1);
    vec3 white = vec3(1);
    vec3 blue = vec3(0.2,0.3,0.7);
	vec3 background;
	
	for (int i=0; i<nbounce; ++i)
	{
	
        /* intersected nothing -> background */
        if (!intersect_scene (ray, mint, maxt, info))
            return col + throughput*mix(white, blue, ray.direction.y * 0.5 + 0.5);
        
        /* intersected an object -> add emission */
        col += throughput*info.mat.emissive;
        
        /* intersection data */
        vec3 pos = info.pos;
        vec3 normal = info.normal;
        vec3 dir_in = normalize(ray.direction);
        vec3 pos_out;
        vec3 dir_out;
        
        /* cos angle with the normal */
        float cos_view = dot(dir_in, normal);
        /* the absolute value of the cosine */
        float cos_in;
        /* whether the normal needs to be flipped */
        bool flipped_normal = cos_view > 0.0;
        /* indices of refraction (ior) on the inside */
        float eta = info.mat.ior;
        /* ray arrives from the "inside" */
        if (flipped_normal)
        {
            /* cos_theta > 0 */
            cos_in = cos_view;
            /* flip normal */
            normal = -normal;
        }
        else /* ray arrives from the outside */
        {
            cos_in = -cos_view; /* cos_theta <= 0 */
            /* flip ior ratio, assume outside is always air (1.0) */
            eta = 1.0/eta;
        }
        
        /* Handle different materials */
        switch (info.mat.type)
        {
        case 0: /* Lambert */
            /* offset to upper hemisphere to avoid self-intersection */
            pos_out = pos + EPSILON * normal;
            /* scatter cosine weighted */
            dir_out = mat_scatter_Lambert_cos(normal);
            throughput *= mat_eval_Lambert_cos(info.mat.base_color*INV_PI);
            break;
            
        case 1: /* perfect mirror */
            /* offset to upper hemisphere to avoid self-intersection */
            pos_out = pos + EPSILON * normal;
            /* reflect */
            dir_out = dir_in + (cos_in+cos_in)*normal;
            throughput *= mat_eval_mirror(info.mat.base_color);    
            break;
            
        case 2: /* dielectric */
        {
            float cos_out_sqr = 1.0 - eta*eta * (1.0-cos_in*cos_in);
			float cos_out, f_refl;
			
			bool refl = (cos_out_sqr<=0);
			if(!refl) {
				/* refraction cosine */
				cos_out = sqrt(max(0,cos_out_sqr));
				/* Fresnel reflectance */
				f_refl = frensel_reflectance(cos_in,cos_out,eta);
            
				/* total internal reflection or Fresnel reflectance */
				refl = (rand() < f_refl);
			}
            
            if (refl)
            {
                /* upper hemisphere offset */
                pos_out = pos + EPSILON * normal;       
                /* reflection */
                dir_out = dir_in + (cos_in + cos_in)*normal;
            }
            else
            {
                /* lower hemisphere offset */
                pos_out = pos - EPSILON * normal;
                /* refraction, cos_in = -dot(d,n) */
                dir_out = eta*dir_in + (eta*cos_in-cos_out)*normal;
            }
            throughput *= mat_eval_dielectric(info.mat.base_color);   
            break;
        }
        default:
            return vec3(0);
        }
        
        /* prepare next ray */
        ray = Ray(pos_out, dir_out);
	}
	
	/* return black if it runs out of iterations */
	return vec3(0);

} /* integrator_Kajiya */
	 
/*--------------------------------------------------------------------------*/

vec3 integrator_Hart

	(Ray   primary_ray, /* primary ray */
	 float mint,        /* lower bound for t */
	 float maxt)        /* upper bound for t */
     
{

    IsectM info;
    intersect_scene_st(primary_ray, mint, maxt, info);
    return vec3(float(info.iter) / float(MARCH_ITER-1));
 
} /* integrator_Hart */

/*--------------------------------------------------------------------------*/




