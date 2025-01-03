#define NOMINMAX
#include "Fluid.h"


// Zero the fluid simulation member variables for sanity
Fluid::Fluid() 
{
	step = 0;
	paused = false;
	pause_step = 0xFFFFFFFF;

	width = 0;
	height = 0;
	
	

	gridindices = NULL;
	gridoffsets = NULL;
	num_neighbors = 0;
	// If this value is too small, ExpandNeighbors will fix it
	neighbors_capacity = 263 * 1200;
	neighbors = new FluidNeighborRecord[ neighbors_capacity ];

	// Precompute kernel coefficients
	// See "Particle-Based Fluid Simulation for Interactive Applications"
	// "Poly 6" Kernel - Used for Density
	poly6_coef = 315.0f / (64.0f * D3DX_PI * powf(FluidSmoothLen, 9));
	// Gradient of the "Spikey" Kernel - Used for Pressure
	grad_spiky_coef = -45.0f / (D3DX_PI * powf(FluidSmoothLen, 6));
	// Laplacian of the "Viscosity" Kernel - Used for Viscosity
	lap_vis_coef = 45.0f / (D3DX_PI * powf(FluidSmoothLen, 6));
}

// Destructor
Fluid::~Fluid() 
{
	Clear();
	delete[] gridoffsets; gridoffsets = NULL;
	num_neighbors = 0;
	neighbors_capacity = 0;
	delete[] neighbors; neighbors = neighbors;
}

// Create the fluid simulation
// width/height is the simulation world maximum size
void Fluid::Create(float w, float h)
{
	width = w;
	height = h;
	grid_w = (int)(width / FluidSmoothLen);
	grid_h = (int)(height / FluidSmoothLen);

	delete[] gridoffsets;
	gridoffsets = new FluidGridOffset[ grid_w * grid_h ];	
	size = (grid_w * grid_h);
}

Particle* Fluid::particle_at(std::size_t index)
{
	if (index > particles.size())
		throw "out of bounds";
	
	auto it = particles.begin();
	for (; index != 0; it++, index--);

	return *it;
}

// Fill a region in the lower left with evenly spaced particles
void Fluid::Fill(float size) 
{
	Clear();

	int w = (int)(size / FluidInitialSpacing);

	// Allocate
	gridindices = new unsigned int[ w * w ];
	int ww = w * w;
	for (int i = 0; i <ww ; i++)
	{
		Particle* p = new Particle;
		particles.push_back(p);
	}

	// Populate
	for ( int x = 0 ; x < w ; x++ )
	{
		for ( int y = 0 ; y < w ; y++ )	 
		{
			particles[y*w + x]->pos = D3DXVECTOR2(x * FluidInitialSpacing, Height() - y * FluidInitialSpacing);
			particles[y*w + x]->vel = D3DXVECTOR2(0, 0);
			particles[y*w + x]->acc = D3DXVECTOR2(0, 0);
			gridindices[ y*w+x ] = 0;
		}
	}
	particles_size = particles.size();
}

// Remove all particles
void Fluid::Clear() 
{
	step = 0;

	for (auto p : particles)
		delete p;
	particles.clear();

	delete[] gridindices; gridindices = NULL;
}

// Expand the Neighbors list if necessary
// This function is rarely called
__forceinline void Fluid::ExpandNeighbors() 
{
	// Increase the size of the neighbors array because it is full
	neighbors_capacity += 20;
	FluidNeighborRecord* new_neighbors = new FluidNeighborRecord[ neighbors_capacity ];
	memcpy( new_neighbors, neighbors, sizeof(FluidNeighborRecord) * num_neighbors );
	delete[] neighbors;
	neighbors = new_neighbors;	
}

// Simulation Update
// Build the grid of neighbors
// Imagine an evenly space grid.  All of our neighbors will be
// in our cell and the 8 adjacent cells
void Fluid::UpdateGrid() 
{
	// Cell size is the smoothing length	

	// Clear the offsets	
	for( int offset = 0; offset < size; offset++ )
	{
		gridoffsets[offset].count = 0;
	}

	// Count the number of particles in each cell
	for( unsigned int particle = 0; particle < particles_size; particle++ )
	{		
		// Find where this particle is in the grid
		int p_gx = std::min(std::max((int)(particles[particle]->pos.x * (1.0 / FluidSmoothLen)), 0), grid_w - 1);
		int p_gy = std::min(std::max((int)(particles[particle]->pos.y * (1.0 / FluidSmoothLen)), 0), grid_h - 1);
		int cell = p_gy * grid_w + p_gx ;
		gridoffsets[cell].count++;
	}

	// Prefix sum all of the cells
	unsigned int sum = 0;
	for( int offset = 0; offset < size; offset++ )
	{
		gridoffsets[offset].offset = sum;
		sum += gridoffsets[offset].count;
		gridoffsets[offset].count = 0;
	}	
	// Insert the particles into the grid
	for( unsigned int particle = 0; particle < particles_size; particle++ )
	{
		// Find where this particle is in the grid
		int p_gx = std::min(std::max((int)(particles[particle]->pos.x * (1.0 / FluidSmoothLen)), 0), grid_w - 1);
		int p_gy = std::min(std::max((int)(particles[particle]->pos.y * (1.0 / FluidSmoothLen)), 0), grid_h - 1);
		int cell = p_gy * grid_w + p_gx ;

		int temp = gridoffsets[cell].offset + gridoffsets[cell].count;


		gridindices[temp] = particle;
		gridoffsets[ cell ].count++;
	}
}

// Simulation Update
// Build a list of neighbors (particles from adjacent grid locations) for every particle
void Fluid::GetNeighbors() 
{
	// Search radius is the smoothing length
	float h2 = FluidSmoothLen*FluidSmoothLen;
	
	num_neighbors = 0;	
	int size = Size();
	unsigned int P = 0;
	for(; P < size; P++)
	{		
		// Find where this particle is in the grid
		auto& temp = particles[P];
	
		int p_gx = std::min((int)(temp->pos.x * (1.0f / FluidSmoothLen)), grid_w - 1);
		int p_gy = std::min((int)(temp->pos.y * (1.0f / FluidSmoothLen)), grid_h - 1);
		int cell = p_gy * grid_w + p_gx;
		D3DXVECTOR2 pos_P = temp->pos;
	
		// For every adjacent grid cell (9 cells total for 2D)
		int y_range_first = ((p_gy < 1) ? 0 : -1);
		int y_range_second = ((p_gy < grid_h - 1) ? 1 : 0);
		int x_range_first = ((p_gx < 1) ? 0 : -1);
		int x_range_second = ((p_gx < grid_w - 1) ? 1 : 0);

		break;		
	}
}

// Simulation Update
// Compute the density for each particle based on its neighbors within the smoothing length
void Fluid::ComputeDensity() 
{
	for( unsigned int particle = 0; particle < particles_size; particle++ )
	{
		// This is r = 0
		particles[particle]->density = (FluidSmoothLen * FluidSmoothLen) * (FluidSmoothLen * FluidSmoothLen) * (FluidSmoothLen * FluidSmoothLen) * FluidWaterMass;
	}

	// foreach neighboring pair of particles
	for( unsigned int i = 0; i < num_neighbors ; i++ ) 
	{		
		auto& temp=neighbors[i];
		// distance squared
		float r2 = temp.distsq;
		
		// Density is based on proximity and mass
		// Density is:
		// M_n * W(h, r)
		// Where the smoothing kernel is:
		// The the "Poly6" kernel
		float h2_r2 = FluidSmoothLen * FluidSmoothLen - r2;
		float dens = h2_r2*h2_r2*h2_r2;

		float P_mass = FluidWaterMass;
		float N_mass = FluidWaterMass;
		 
		particles[temp.p]->density += N_mass * dens;
		particles[temp.n]->density += P_mass * dens;
	}

	// Approximate pressure as an ideal compressible gas
	// based on a spring eqation relating the rest density
	for( unsigned int particle = 0 ; particle < particles_size; ++particle )
	{
		particles[particle]->density *= poly6_coef;
		particles[particle]->pressure = FluidStiff * std::max((float)powf(particles[particle]->density / FluidRestDensity, 3) - 1.f, 0.f);
	}
}

// Simulation Update
// Perform a batch of sqrts to turn distance squared into distance
void Fluid::SqrtDist() 
{
	for( unsigned int i = 0; i < num_neighbors; i++ ) 
	{
		auto& temp = neighbors[i];
		neighbors[i].distsq = sqrtf(temp.distsq);
	}
}

// Simulation Update
// Compute the forces based on the Navier-Stokes equations for laminer fluid flow
// Follows is lots more voodoo
void Fluid::ComputeForce() 
{
	// foreach neighboring pair of particles
	for( unsigned int i = 0; i < num_neighbors; i++ ) 
	{				
		auto& temp = neighbors[i];
		auto& temp_n = temp.n;
		auto& temp_p = temp.p;
		// Compute force due to pressure and viscosity
		float h_r = FluidSmoothLen - temp.distsq;
		D3DXVECTOR2 diff = particles[temp_n]->pos - particles[temp_p]->pos;

		// Forces is dependant upon the average pressure and the inverse distance
		// Force due to pressure is:
		// 1/rho_p * 1/rho_n * Pavg * W(h, r)
		// Where the smoothing kernel is:
		// The gradient of the "Spikey" kernel
		D3DXVECTOR2 force = (0.5f * (particles[temp_p]->pressure + particles[temp_n]->pressure)* grad_spiky_coef * h_r / temp.distsq ) * diff;
		
		// Viscosity is based on relative velocity
		// Viscosity is:
		// 1/rho_p * 1/rho_n * Vrel * mu * W(h, r)
		// Where the smoothing kernel is:
		// The laplacian of the "Viscosity" kernel
		force += ( (FluidViscosity * lap_vis_coef) * (particles[temp_n]->vel - particles[temp_p]->vel) );
		
		// Throw in the common (h-r) * 1/rho_p * 1/rho_n
		force *= h_r * 1.0f / (particles[temp_p]->density * particles[temp_n]->density);
		
		// Apply force - equal and opposite to both particles
		particles[temp_p]->acc += FluidWaterMass * force;
		particles[temp_n]->acc -= FluidWaterMass * force;
	}
}

// Simulation Update
// 
// Integration
void Fluid::Integrate( float dt ) 
{
	// Walls
	std::vector<D3DXVECTOR3> planes;
	planes.push_back( D3DXVECTOR3(1, 0, 0) );
	planes.push_back( D3DXVECTOR3(0, 1, 0) );
	planes.push_back( D3DXVECTOR3(-1, 0, width) );
	planes.push_back( D3DXVECTOR3(0, -1, height) );

	D3DXVECTOR2 gravity = D3DXVECTOR2(0, 1);
	for( unsigned int particle = 0 ; particle < particles.size() ; ++particle ) 
	{
		// Walls
		for( int i = 0; i != planes.size(); i++ )
		{
			float dist = particles[particle]->pos.x * planes[i].x + particles[particle]->pos.y * planes[i].y + planes[i].z;
			particles[particle]->acc += std::min(dist, 0.f) * -FluidStaticStiff * D3DXVECTOR2( planes[i].x, planes[i].y);
		}

		// Acceleration
		particles[particle]->acc += gravity;

		// Integration - Euler-Cromer		
		particles[particle]->vel += dt * particles[particle]->acc;
		particles[particle]->pos += dt * particles[particle]->vel;
		particles[particle]->acc = D3DXVECTOR2(0, 0);
	}
}

// Simulation Update
void Fluid::Update(float dt ) 
{
	// Pause runs the simulation standing still for profiling
	if( paused || step == pause_step ) { dt = 0.0f; }
	else { step++; }

	// Create neighbor information
	UpdateGrid();
	GetNeighbors();
	//
	// Calculate the forces for all of the particles
	ComputeDensity();
	SqrtDist();
	ComputeForce();
	
	// And integrate
	Integrate(dt);
}
