# mcmd
This is a Monte Carlo / Molecular Dynamics Simulation software.<br />

--> MC Currently fully supporting NPT,NVT,NVE,uVT ensembles.  <br />
--> MD supporting NVT.  <br />
--> POLARIZATION IS NOT WORKING so avoid using  <br />
    `potential_form   ljespolar`<br /><br />

PRE-COMPILED EXECUTABLE WORKS WITH THE FOLLOWING COMPILERS:  <br />
    -> gcc compiler 6.2.0 (circe)  <br />
    -> gcc compiler 4.9.3 (stampede)  <br />

To compile:  (in src dir "src")<br />
`g++ main.cpp -lm -o ../t -I. -std=c++11`  <br />

To run (in base dir "mcmd") <br />
`./t mcmd.inp`<br /><br />  
  
<hr />
  
TODO<br /><br />

-> for some reason my RD energy is 0.001% off from MPMC every time. Maybe self energy is diff?<br />
-> allow 0 sorbate molecules in uvt<br />
-> make pair lists for MC to run faster. <br />
-> fix MC SD's maybe? Seem to be wrong<br />
    -> NEED TO CHECK IF 4TH POINT OF PLANE MAKES THE SAME PLANE AS PREVIOUS 3 BEFORE MOVING ON<br />
-> REDUCE CHARGE CALCULATIONS BY CONVERTING TO AU BEFOREHAND AND CONVERTING BACK IN OUTPUT<br />
-> make correct pressure calculator (Frenkel method, check MPMC to compare)<br />  
-> include more-than-static polarization energy  <br />
	-> right now I just use V = -0.5 sum{u.E}  <br />
-> Use GPU for MD force calculations? (add option)  <br />
-> Use GPU for polarization routine (later)  <br />
-> Implement Phast2 model?  <br />
