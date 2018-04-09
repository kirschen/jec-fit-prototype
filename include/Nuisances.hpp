#pragma once

#include <TF1.h>
#include <iostream>
#include <sstream>

/**
 * \struct Nuisances
 * \brief Nuisance parameters
 * 
 * Nuisance parameters that affect measurements of jet corrections are defined as publicly
 * accessible data members of this structure.
 */
struct Nuisances
{
    /**
     * \brief Default constructor
     * 
     * Initializes all nuisance parameters with values that correspond to the nominal configuration
     * (usually this means zeros).
     */
    Nuisances();
    
    /**
     * \brief Relative difference between photon pt scale in data and simulation
     * 
     * Defined such that ptData = (1 + photonScale) * ptSim.
     */
    double photonScale;


    /**
     *
     *
     */
  double MPF_JEC;
  double MJB_JEC;
  double MPF_JER;
  double MJB_JER;
  double MPF_PU;
  double MJB_PU;

  double MPF_FSR;
  double MJB_FSR;

  TF1 MPF_JECFunc;
  TF1 MJB_JECFunc;
  TF1 MPF_JERFunc;
  TF1 MJB_JERFunc;
  TF1 MPF_PUFunc;
  TF1 MJB_PUFunc;

  TF1 MPF_FSRFunc;
  TF1 MJB_FSRFunc;
  std::vector<std::tuple<std::string, double*, TF1*> > MJB_NuisanceCollection;

  std::vector<std::tuple<std::string, double*, TF1*> > MPF_NuisanceCollection;

  std::vector<std::tuple<std::string, double*, TF1*> > Multijet_NuisanceCollection;
  
};
