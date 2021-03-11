/**
 * @file    modify_orbits_forces.c
 * @brief   Update orbital elements with prescribed timescales using forces.
 * @author  Dan Tamayo <tamayo.daniel@gmail.com>
 * 
 * @section     LICENSE
 * Copyright (c) 2015 Dan Tamayo, Hanno Rein
 *
 * This file is part of reboundx.
 *
 * reboundx is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * reboundx is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rebound.  If not, see <http://www.gnu.org/licenses/>.
 *
 * The section after the dollar signs gets built into the documentation by a script.  All lines must start with space * space like below.
 * Tables always must be preceded and followed by a blank line.  See http://docutils.sourceforge.net/docs/user/rst/quickstart.html for a primer on rst.
 * $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
 *
 * $Orbit Modifications$       // Effect category (must be the first non-blank line after dollar signs and between dollar signs to be detected by script).
 *
 * ======================= ===============================================
 * Authors                 D. Tamayo, H. Rein
 * Implementation Paper    `Kostov et al., 2016 <https://ui.adsabs.harvard.edu/abs/2016ApJ...832..183K/abstract>`_.
 * Based on                `Papaloizou & Larwood 2000 <http://labs.adsabs.harvard.edu/adsabs/abs/2000MNRAS.315..823P/>`_.
 * C Example               :ref:`c_example_modify_orbits`
 * Python Example          `Migration.ipynb <https://github.com/dtamayo/reboundx/blob/master/ipython_examples/Migration.ipynb>`_
 *                         `EccAndIncDamping.ipynb <https://github.com/dtamayo/reboundx/blob/master/ipython_examples/EccAndIncDamping.ipynb>`_.
 * ======================= ===============================================
 * 
 * This applies physical forces that orbit-average to give exponential growth/decay of the semimajor axis, eccentricity and inclination.
 * The eccentricity damping keeps the angular momentum constant (corresponding to `p=1` in modify_orbits_direct), which means that eccentricity damping will induce some semimajor axis evolution.
 * Additionally, eccentricity/inclination damping will induce pericenter/nodal precession.
 * Both these effects are physical, and the method is more robust for strongly perturbed systems.
 * 
 * This is similar to the modify_orbit_force but with an added effect of an inner disc edge.
 * 
 * **Effect Parameters**
 *
 * If coordinates not, defaults to using Jacobi coordinates.
 *
 * ============================ =========== ==================================================================
 * Field (C type)               Required    Description
 * ============================ =========== ==================================================================
 * coordinates (enum)           No          Type of elements to use for modification (Jacobi, barycentric or particle).
 *                                          See the examples for usage.
 * h                            Yes         The width of the inner disc edge
 * dedge                        Yes         The position of the inner disc edge
 * ============================ =========== ==================================================================
 *
 * **Particle Parameters**
 *
 * One can pick and choose which particles have which parameters set.  
 * For each particle, any unset parameter is ignored.
 *
 * ============================ =========== ==========================================================================
 * Field (C type)               Required    Description
 * ============================ =========== ==========================================================================
 * tau_a (double)               No          Semimajor axis exponential growth/damping timescale
 * tau_e (double)               No          Eccentricity exponential growth/damping timescale
 * tau_inc (double)             No          Inclination axis exponential growth/damping timescale
 * tau_a_red (double)           No         A factor used to revrse inward migration, illustrating an inner disc edge 
 * ============================ =========== ==========================================================================
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rebound.h"
#include "reboundx.h"
#include "rebxtools.h"

//with a planet trap at inner disc edge to reverse the migration and prevent migration onto the star
const double rebx_calculate_planet_trap(const double r, const double h, const double dedge){
    double tau_a_red;

    if (r > dedge*(1.0 + h)){
        tau_a_red = 1.0;
    }

    else if (dedge*(1.0 - h) < r){
        tau_a_red =  5.5 * cos( ((dedge * (1.0 + h) - r ) * 2 * M_PI) / (4 * h * dedge) ) - 4.5;
    }

    else {
        tau_a_red = -10.0;
    }

    return tau_a_red;
}

/* 
The function above defines and calculates the tau_a_red parameters which illustrates a planet trap, in form of migration reversal. 
This function will be called and used in the function below. In that same function the parameters or arguments tau_a_red needs are decleared or retrieved
The function for tau_a_red is as described in Pichierri 2018. 
This file will then be added as a file of its own to reboundx when finished, and can be called in a simulation

Then I will set a value of the parameters in problem.c file as: rebx_set_param_double(rebx, &...(?).ap, "disc_edge", 0.1); and 
rebc_set_param_double(rebx, &...(?).ap, "width", 0.2);


Lastly I need to add the new effect as else if statements in the core.c and core.h
    else if (strcmp(name, "stark_force") == 0){
    force->update_a = rebx_stark_force;  (not a force though right? But it will affect the position not velocity or acceleration at least)
    force->force_type = REBX_FORCE_POS;
    }
*/

/* defining the disc inner edge as a float value and setting it to the general, maybe when registering these in the main file, the user can set the values outside when doing the simulation, such as in python in simsetup???
    double dedge = 0.1; 
    rebx_set_param_double(rebx, &force->ap, "disc_edge", dedge);  
    double h = 0.2;       // defining the width of the edge
    rebx_set_param_double(rebx, &force->ap, "width", h); 
*/

static struct reb_vec3d rebx_calculating_orbits_with_inner_disc_edge(struct reb_simulation* const sim, struct rebx_force* const force, struct reb_particle* p, struct reb_particle* source){
    double invtau_a = 0.0;
    double tau_e = INFINITY;
    double tau_inc = INFINITY;
    
    const double* const tau_a_ptr = rebx_get_param(sim->extras, p->ap, "tau_a");
    const double* const tau_e_ptr = rebx_get_param(sim->extras, p->ap, "tau_e");
    const double* const tau_inc_ptr = rebx_get_param(sim->extras, p->ap, "tau_inc");
    const double* const dedge = rebx_get_param(sim->extras, force->ap, "inner_disc_edge");
    const double* const h = rebx_get_param(sim->extras, force->ap, "disc_edge_width");

    const double dvx = p->vx - source->vx;
    const double dvy = p->vy - source->vy;
    const double dvz = p->vz - source->vz;
    const double dx = p->x-source->x;
    const double dy = p->y-source->y;
    const double dz = p->z-source->z;
    const double r2 = dx*dx + dy*dy + dz*dz;

    int err=0;
    struct reb_orbit o = reb_tools_particle_to_orbit_err(sim->G, *p, *source, &err);
    /*if(err){        // mass of primary was 0 or p = primary.  Return same particle without doing anything.
        return *p;
    }*/
    const double a0 = o.a;

    if(tau_a_ptr != NULL){
        invtau_a = rebx_calculate_planet_trap(a0, *h, *dedge)/(*tau_a_ptr);
    }
    if(tau_e_ptr != NULL){
        tau_e = *tau_e_ptr;
    }
    if(tau_inc_ptr != NULL){
        tau_inc = *tau_inc_ptr;
    }
    
    struct reb_vec3d a = {0};

    a.x =  dvx*invtau_a/(2.);
    a.y =  dvy*invtau_a/(2.);
    a.z =  dvz*invtau_a/(2.);

    if (tau_e < INFINITY || tau_inc < INFINITY){
        const double vdotr = dx*dvx + dy*dvy + dz*dvz;
        const double prefac = 2*vdotr/r2/tau_e;
        a.x += prefac*dx;
        a.y += prefac*dy;
        a.z += prefac*dz + 2.*dvz/tau_inc;
    }
    return a;
}


void rebx_inner_disc_edge(struct reb_simulation* const sim, struct rebx_force* const force, struct reb_particle* const particles, const int N){
    int* ptr = rebx_get_param(sim->extras, force->ap, "coordinates");
    enum REBX_COORDINATES coordinates = REBX_COORDINATES_JACOBI; // Default
    if (ptr != NULL){
        coordinates = *ptr;
    }
    const int back_reactions_inclusive = 1;
    const char* reference_name = "primary";
    rebx_com_force(sim, force, coordinates, back_reactions_inclusive, reference_name, rebx_calculating_orbits_with_inner_disc_edge, particles, N);
}
