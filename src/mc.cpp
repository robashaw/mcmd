#include <string>
#include <algorithm>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <map>
#include <string>
#include <stdlib.h>
#include <potential.cpp>
#include <add_molecule.cpp>
#include <remove_molecule.cpp>
#include <boltzmann.cpp>

// PHAST2 NOT INCLUDED YET

/* (RE)DEFINE THE BOX LENGTHS */
void defineBox(System &system) { // takes input in A
	// easy 90 90 90 systems
	if (system.pbc.alpha == 90 && system.pbc.beta == 90 && system.pbc.gamma == 90) {
		// assumes x_length, y_length, z_length are defined already in system.
		// i.e. the volume-change function does that before calling this function

		system.pbc.basis[0][0] = system.pbc.x_length;
		system.pbc.basis[1][1] = system.pbc.y_length;   
		system.pbc.basis[2][2] = system.pbc.z_length;

		system.pbc.x_max = system.pbc.x_length/2.0;
		system.pbc.x_min = -system.pbc.x_max;
		system.pbc.y_max = system.pbc.y_length/2.0;
		system.pbc.y_min = -system.pbc.y_max;
		system.pbc.z_max = system.pbc.z_length/2.0;
		system.pbc.z_min = -system.pbc.z_max;

		system.pbc.cutoff = system.pbc.x_max; // crude method but good for cubes (and only cubes!!)
		// need to make basis vector system later.
		system.constants.ewald_alpha = 3.5/system.constants.cutoff; // update ewald_alpha if we have a vol change

		system.pbc.volume = system.pbc.x_length * system.pbc.y_length * system.pbc.z_length; // in A^3
	}
	// universal definitions
	else {
		// this could forseeably be a problem if, for some weird reason, someone wants to do NPT with a weird box.
		system.pbc.calcVolume();
		system.pbc.calcRecip();
		system.pbc.calcCutoff();
		system.pbc.calcBoxVertices();
		system.pbc.calcPlanes();
	}
}

/* CHANGE VOLUME OF SYSTEM BY BOLTZMANN PROB -- FOR NPT */
void changeVolumeMove(System &system) {
	// increasing volume will increase energy supplied by external pressure but also allow more room for lower potential among atoms
	system.stats.volume_attempts++;
	// generate small randam distance change for volume adjustment
	double ranf = (double)rand() / (double)RAND_MAX; // for boltz check	
    double ranv = (double)rand() / (double)RAND_MAX; // for volume change
    double old_energy, new_energy;

        double* energies = getTotalPotential(system,system.constants.potential_form);
        old_energy=energies[0]+energies[1]+energies[2]+energies[3];

    double old_side = system.pbc.x_length; // in A; save just in case, to reset if move rejected.
    system.pbc.old_volume = system.pbc.volume;

    // change the volume to test new energy.
    double new_volume = exp(log(system.pbc.volume) + (ranv-0.5)*system.constants.volume_change);// mpmc default = 2.5

    //double new_volume = 1e-30*new_volume_A3;
    double new_side = pow(new_volume, 1.0/3.0);
    system.pbc.x_length = new_side;
    system.pbc.y_length = new_side;
    system.pbc.z_length = new_side;
    defineBox(system);
    //printf("OLDV before energy call: %f; NEWV before energy call: %f\n", system.pbc.old_volume, system.pbc.volume);        
        double* potentials = getTotalPotential(system,system.constants.potential_form);
        new_energy=potentials[0]+potentials[1]+potentials[2]+potentials[3];

    //printf("OLDV before bf call: %f; NEWV before bf call: %f\n", system.pbc.old_volume, system.pbc.volume); 
    double boltzmann_factor = get_boltzmann_factor(system, old_energy, new_energy, "volume");
	system.stats.volume_change_bf_sum += boltzmann_factor;

    //printf("ranf < bf? %f < %f ?\n", ranf, boltzmann_factor);
	if (ranf < boltzmann_factor) {
		// accept move
        //printf("ACCEPTED\n");
		system.stats.volume_change_accepts++;
	} else {
        //printf("REJECTED\n");
		// reject move (move volume back)
        system.pbc.x_length = old_side;
        system.pbc.y_length = old_side;
        system.pbc.z_length = old_side;
        defineBox(system);
	}
}


// ================== MAIN MC FUNCTION. HANDLES MOVE TYPES AND BOLTZMANN ACCEPTANCE =================
// ACCORDING TO DIFFERENT ENSEMBLES
void runMonteCarloStep(System &system, string model) {
    system.checkpoint("Entered runMonteCarloStep().");

	// VOLUME MOVE (only NPT)
	if (system.constants.ensemble == "npt") {
		double VCP = system.constants.vcp_factor/(double)system.stats.count_movables; // Volume Change Probability
		double ranf = (double)rand() / (double)RAND_MAX; // between 0 and 1
        	if (ranf < VCP) {
                    system.checkpoint("doing a volume change move.");
                	changeVolumeMove(system);
                    system.checkpoint("done with volume change move.");
			return; // we tried a volume change, so exit MC step.
		}
	}

	// ADD / REMOVE (only uVT)
	// We'll choose 0-0.5 for add; 0.5-1 for remove (equal prob.)
	if (system.constants.ensemble == "uvt") {
		double IRP = system.constants.insert_factor;  
		double ranf = (double)rand() / (double)RAND_MAX; // between 0 and 1
		if (ranf < IRP) {
            //system.checkpoint("doing an add or remove.");
			// we're going to do an add/remove now.
			double ranf2 = (double)rand() / (double)RAND_MAX; // 0->1
			// ADD A MOLECULE
			if (ranf2 < 0.5) {
                system.checkpoint("doing molecule add move.");
				addMolecule(system, model);			
                system.checkpoint("done with molecule add move.");
			} // end add
 
			else { // REMOVE MOLECULE
                system.checkpoint("doing molecule delete move.");
				removeMolecule(system, model);
                system.checkpoint("done with molecule delete move.");
			} // end add vs. remove		
		return; // we did the add or remove so exit MC step.
		} // end doing an add/remove. 
	} // end if uVT		

	
	// DISPLACE / ROTATE (for all: NPT, uVT, NVT, NVE); NVE has special BoltzFact tho.
	// make sure it's a movable molecule
	system.checkpoint("NOT volume/add/remove :: Starting displace or rotate..");
    string movable="notyet";
    int randm = -1;
    while (movable != "M") {
            randm = (rand() % (int)(system.stats.count_movables)) + (int)system.stats.count_frozen_molecules;
            //printf("randm: %i\n",randm);
            movable = system.molecules[randm].MF;
    }           
	system.checkpoint("Got the random molecule.");
	
	double old_V=0.0; double new_V=0.0;

	// first calculate the system's current potential energy
		double* oldpotentials = getTotalPotential(system,model); //new double[2];
		old_V = (oldpotentials[0]+oldpotentials[1]+oldpotentials[2]+oldpotentials[3]);	// keep in K
        //printf("0 1 2 3: %f %f %f %f\n", oldpotentials[0], oldpotentials[1], oldpotentials[2], oldpotentials[3]);
        
    // save a temporary copy of molecule to go back if needed
    Molecule tmp_molecule = system.molecules[randm];

	// pick rotation or displacement
	int rot_disp_flag = -1;  // THIS WILL BE 0 FOR ROTATION, 1 FOR TRANSLATION
	double randRotTrans = (double)rand()/(double)RAND_MAX;
	if (randRotTrans < system.constants.rotate_prob && system.constants.rotate_option == "on") { // do a rotation
		
        // ROTATION
        system.checkpoint("doing a rotation move.");
		rot_disp_flag = 0; system.stats.rotate_attempts++;
		double randangle; string plane; 
		randangle = system.constants.rotate_angle_factor*(double)rand()/(double)RAND_MAX; // angle of rotation from 0 -> rotate_angle_factor
		double randxyz = (double)rand()/(double)RAND_MAX;
		// 1/3 change for a given plane
		if (randxyz < 0.33333) {
			plane="x";
		} else if (randxyz > 0.33333 && randxyz < 0.66667) {
			plane="y";
		} else { plane="z";}

		for (int i=0; i<system.molecules[randm].atoms.size(); i++) {
				double* rotated = rotatePoint(system, system.molecules[randm].atoms[i].pos[0], system.molecules[randm].atoms[i].pos[1], system.molecules[randm].atoms[i].pos[2], plane, randangle);
				system.molecules[randm].atoms[i].pos[0] = rotated[0];
				system.molecules[randm].atoms[i].pos[1] = rotated[1];
				system.molecules[randm].atoms[i].pos[2] = rotated[2];
		} // end for i
        //printf("rotate success.\n");
	} // end rotation option 
	else {
		// DISPLACEMENT
		rot_disp_flag = 1; system.stats.displace_attempts++;
        system.checkpoint("doing displacement move.");
		double randx,randy,randz;
        	randx = system.constants.displace_factor * (((double)rand() / (double)RAND_MAX)*2-1);
        	randy = system.constants.displace_factor * (((double)rand() / (double)RAND_MAX)*2-1);
        	randz = system.constants.displace_factor * (((double)rand() / (double)RAND_MAX)*2-1);

			for (int i=0; i<system.molecules[randm].atoms.size(); i++) {
					system.molecules[randm].atoms[i].pos[0] += randx;
        			system.molecules[randm].atoms[i].pos[1] += randy;
        			system.molecules[randm].atoms[i].pos[2] += randz;
			}
    } // end rotation/displacement if/else

	// check P.B.C. (move the molecule back in the box if needed)
    checkInTheBox(system, randm);

		double* newpotentials = getTotalPotential(system,model); //new double[2];
                new_V = (newpotentials[0]+newpotentials[1]+newpotentials[2]+newpotentials[3]); // keep in K

	double potential_diff = new_V - old_V;

	// now accept or reject the move based on Boltzmann probability
	double boltzmann_factor;
	if (system.constants.ensemble != "nve") {
	        boltzmann_factor = exp(-potential_diff/(system.constants.temp)); // no kb because energy is already in K
	   
	} else if (system.constants.ensemble == "nve") { // for nve only
		boltzmann_factor = pow((system.constants.total_energy - new_V),3.0*system.stats.count_movables/2.0) / pow((system.constants.total_energy - old_V),(3.0*system.stats.count_movables/2.0)); // again, no kb because energy already in K

	}    
            if (boltzmann_factor != INFINITY) {
            if (rot_disp_flag == 0)
                system.stats.rotate_bf_sum += boltzmann_factor;
            else if (rot_disp_flag == 1)
                system.stats.displace_bf_sum += boltzmann_factor;
            } // end BF not inf

	// make ranf for probability pick
	double ranf = (double)rand() / (double)RAND_MAX; // a value between 0 and 1
	//printf("ranf: %10f\n",ranf);

	// apply selection Frenkel Smit p. 30 
	// accept move
	if (ranf < boltzmann_factor) {
		if (rot_disp_flag == 0)
			system.stats.rotate_accepts++;
		else if (rot_disp_flag == 1)
			system.stats.displace_accepts++;
		//printf("accepted\n");
	}
	// reject move (by moving it back)
	else {
		// for whole molecule
		for (int i=0; i<system.molecules[randm].atoms.size(); i++) {
			system.molecules[randm].atoms[i].pos[0] = tmp_molecule.atoms[i].pos[0];
            system.molecules[randm].atoms[i].pos[1] = tmp_molecule.atoms[i].pos[1];
            system.molecules[randm].atoms[i].pos[2] = tmp_molecule.atoms[i].pos[2];
		}

            // check P.B.C. (move the molecule back in the box if needed)
            checkInTheBox(system, randm);
	
	} // end displace/rotate (all ensembles)
	system.checkpoint("done with displace/rotate");
    return; // done with move, so exit MC step	
}
