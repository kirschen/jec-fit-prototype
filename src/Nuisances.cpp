#include <Nuisances.hpp>


Nuisances::Nuisances():
  photonScale(0.),
  MJB_JER(1.),
  MJB_JEC(1.),
  MJB_FSR(1.),
  MJB_PU(1.),
  MJB_JERFunc ("MJB_JERFunc","0.01",10,7000),
  MJB_JECFunc ("MJB_JECFunc","0.01",10,7000),
  MJB_FSRFunc ("MJB_FSRFunc","0.01",10,7000),
  MJB_PUFunc  ("MJB_PUFunc","0.01",10,7000),

  MPF_JER(1.),
  MPF_JEC(1.),
  MPF_FSR(1.),
  MPF_PU(1.),
  MPF_JERFunc ("MPF_JERFunc","0.01*log(x)",10,7000),
  MPF_JECFunc ("MPF_JECFunc","0.01",10,7000),
  MPF_FSRFunc ("MPF_FSRFunc","0.01*log(x)*log(x)",10,7000),
  MPF_PUFunc  ("MPF_PUFunc","0.01*log(x)*x",10,7000)
  
{
  MJB_Nuisances.push_back(&MJB_JER);
  MJB_Nuisances.push_back(&MJB_JEC);
  MJB_Nuisances.push_back(&MJB_FSR);
  MJB_Nuisances.push_back(&MJB_PU);
  MJB_Parametrisation.push_back(&MJB_JERFunc);
  MJB_Parametrisation.push_back(&MJB_JECFunc);
  MJB_Parametrisation.push_back(&MJB_FSRFunc);
  MJB_Parametrisation.push_back(&MJB_PUFunc);
  MJB_NuisanceCollection.push_back(std::make_tuple("MJB_JER",&MJB_JER,&MJB_JERFunc));
  MJB_NuisanceCollection.push_back(std::make_tuple("MJB_JEC",&MJB_JEC,&MJB_JECFunc));
  MJB_NuisanceCollection.push_back(std::make_tuple("MJB_FSR",&MJB_FSR,&MJB_FSRFunc));
  MJB_NuisanceCollection.push_back(std::make_tuple("MJB_PU",&MJB_PU,&MJB_PUFunc));
  MPF_Nuisances.push_back(&MPF_JER);
  MPF_Nuisances.push_back(&MPF_JEC);
  MPF_Nuisances.push_back(&MPF_FSR);
  MPF_Nuisances.push_back(&MPF_PU);
  MPF_Parametrisation.push_back(&MPF_JERFunc);
  MPF_Parametrisation.push_back(&MPF_JECFunc);
  MPF_Parametrisation.push_back(&MPF_FSRFunc);
  MPF_Parametrisation.push_back(&MPF_PUFunc);
  MPF_NuisanceCollection.push_back(std::make_tuple("MPF_JER",&MPF_JER,&MPF_JERFunc));
  MPF_NuisanceCollection.push_back(std::make_tuple("MPF_JEC",&MPF_JEC,&MPF_JECFunc));
  MPF_NuisanceCollection.push_back(std::make_tuple("MPF_FSR",&MPF_FSR,&MPF_FSRFunc));
  MPF_NuisanceCollection.push_back(std::make_tuple("MPF_PU",&MPF_PU,&MPF_PUFunc));

  Multijet_NuisanceCollection=MJB_NuisanceCollection;
  Multijet_NuisanceCollection.insert(Multijet_NuisanceCollection.end(),MPF_NuisanceCollection.begin(),MPF_NuisanceCollection.end());

  for (auto &i: Multijet_NuisanceCollection){
    std::cout << std::get<std::string>(i).c_str() << std::endl;
    // Do something with i
  }
      
}
